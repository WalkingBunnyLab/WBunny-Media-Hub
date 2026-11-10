#include <memory>
#include "services/bilibili_live.hpp"
#include "core/http.hpp"
#include "core/i18n.hpp"
#include "core/json.hpp"
#include "core/md5.hpp"
#include "core/util.hpp"
#include "player/player.hpp"
#include <algorithm>
#include <climits>
#include <set>
#ifdef BILIBILI_LIVE_TEST_CLIENT
namespace bilibili_live_config { constexpr const char* key="fixture-key"; constexpr const char* secret="fixture-secret"; constexpr const char* build="1"; }
#else
#include "bilibili_live_client.hpp"
#endif

namespace bilibili::live {
namespace {
constexpr const char* API="https://api.live.bilibili.com";
constexpr const char* UA="Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/131.0.0.0 Safari/537.36";
std::string error(json_t* root) {
    if (!json_is_integer(json::at(root,{"code"}))) return "Invalid Bilibili live response";
    auto code=json::num(root,{"code"});
    if (!code) return "";
    if (code==-101) return "Bilibili login expired. Sign in again.";
    if (code==-352 || code==-412) return "Bilibili live requires verification. Try again later.";
    return "Bilibili live " + std::to_string(code) + ": " + json::str(root,{"message"},"Request failed");
}
bool expired(json_t* root) { return json_is_integer(json::at(root,{"code"})) && json::num(root,{"code"})==-101; }
std::string url(std::string s) {
    if (util::starts_with(s,"//")) s="https:"+s;
    if (s.find_first_of("\r\n")!=std::string::npos || (!util::starts_with(s,"https://")&&!util::starts_with(s,"http://"))) return "";
    return s;
}
std::string request(const std::string& path, std::map<std::string,std::string> params, const std::string& cookie,
                    const std::atomic<bool>* cancel, std::string& fail, bool sign=true) {
    http::Request req;
    std::string query;
    if (sign) query=signed_query(std::move(params),util::unix_time()*1000);
    else for (const auto& p:params) { if (!query.empty()) query+='&'; query+=util::url_encode(p.first)+"="+util::url_encode(p.second); }
    req.url=std::string(API)+path+"?"+query;
    req.headers={{"Referer","https://live.bilibili.com/"},{"User-Agent",sign?"wiliwili":UA}};
    if (!cookie.empty()) req.headers.emplace_back("Cookie",cookie);
    req.require_tls=true;req.private_url=true;req.cancel=cancel;req.timeout=15;req.stall_seconds=5;req.max_bytes=4*1024*1024;
    const auto r=http::perform(req);
    if (!r.ok()) { fail=r.error.empty()?"Bilibili live HTTP "+std::to_string(r.status):"Bilibili live network request failed"; return ""; }
    return r.body;
}
Room room(json_t* j) {
    Room r;r.id=json::num(j,{"roomid"},json::num(j,{"room_id"},json::num(j,{"id"})));
    r.uid=json::num(j,{"uid"});r.title=util::html_to_text(json::str(j,{"title"}));r.author=json::str(j,{"uname"});
    r.picture=url(json::str(j,{"cover"},json::str(j,{"cover_from_user"},json::str(j,{"keyframe"}))));
    r.area=json::str(j,{"area_name"},json::str(j,{"area_v2_name"}));
    r.viewers=json::num(j,{"watched_show","num"},json::num(j,{"online"}));return r;
}
}
int64_t room_id(const std::string& input) {
    auto s=util::trim(input);
    for (const char* prefix:{"https://live.bilibili.com/","http://live.bilibili.com/"})
        if (util::starts_with(s,prefix)) { s=s.substr(std::char_traits<char>::length(prefix));s=s.substr(0,s.find_first_of("/?#"));break; }
    if(s.empty()||s.size()>15||s.find_first_not_of("0123456789")!=std::string::npos)return 0;
    int64_t n=0;for(char c:s)n=n*10+c-'0';return n>0?n:0;
}
std::string signed_query(std::map<std::string,std::string> p,int64_t now) {
    p.erase("sign");p["appkey"]=bilibili_live_config::key;p["build"]=bilibili_live_config::build;p["ts"]=std::to_string(now);
    std::string q;for(const auto& v:p){if(!q.empty())q+='&';q+=util::url_encode(v.first)+"="+util::url_encode(v.second);}
    return q+"&sign="+md5::hash(q+bilibili_live_config::secret);
}
Rooms parse_rooms(const std::string& body,int page,bool only_followed) {
    Rooms out;auto doc=json::Doc::parse(body);out.error=error(doc.get());out.auth_expired=expired(doc.get());if(!out.error.empty())return out;
    auto data=json::at(doc.get(),{"data"});auto cards=json::at(data,{"card_list"});auto list=json::at(data,{"list"});
    if(!json_is_array(cards)&&!json_is_array(list)){out.error="Invalid live room list";return out;}
    std::set<int64_t> seen;
    auto add=[&](json_t* j,bool following){auto r=room(j);r.following=following;if(r.id>0&&seen.insert(r.id).second&&out.items.size()<100)out.items.push_back(std::move(r));};
    for(size_t i=0;i<json::size(cards);++i){auto c=json_array_get(cards,i);auto type=json::str(c,{"card_type"});auto d=json::at(c,{"card_data"});
        if(type=="my_idol_v1"){auto a=json::at(d,{"my_idol_v1","list"});for(size_t j=0;j<json::size(a);++j)add(json_array_get(a,j),true);}
        else if(!only_followed&&(type=="small_card_v1"||type=="second_card_v1"))add(json_object_get(d,type.c_str()),false);
    }
    for(size_t i=0;i<json::size(list);++i){auto j=json_array_get(list,i);if(!only_followed||json::num(j,{"live_status"})==1)add(j,only_followed);}
    bool more=json::boolean(data,{"has_more"},false);
    if(only_followed&&json_is_array(list))more=json::num(data,{"live_count"},json::num(data,{"count"}))>int64_t(page)*20;
    if(only_followed&&json_is_array(cards))more=false;
    out.next_page=more&&!out.items.empty()&&page<100?page+1:0;out.ok=true;return out;
}
Areas parse_areas(const std::string& body) {
    Areas out;auto d=json::Doc::parse(body);out.error=error(d.get());if(!out.error.empty())return out;
    auto parents=json::at(d.get(),{"data","list"});if(!json_is_array(parents)){out.error="Invalid live area list";return out;}
    for(size_t i=0;i<std::min<size_t>(json::size(parents),50);++i){auto p=json_array_get(parents,i);int id=(int)json::num(p,{"id"});auto name=json::str(p,{"name"});if(id<=0)continue;out.items.push_back({0,id,name,name});auto list=json::at(p,{"area_list"});if(!json_is_array(list))list=json::at(p,{"list"});for(size_t j=0;j<std::min<size_t>(json::size(list),200)&&out.items.size()<500;++j){auto a=json_array_get(list,j);int aid=(int)json::num(a,{"id"});if(aid>0)out.items.push_back({aid,id,json::str(a,{"name"}),name});}}
    out.ok=true;return out;
}
Playback parse_playback(const std::string& body) {
    Playback out;auto doc=json::Doc::parse(body);out.error=error(doc.get());out.auth_expired=expired(doc.get());if(!out.error.empty())return out;
    auto d=json::at(doc.get(),{"data"});out.room.id=json::num(d,{"room_id"});out.room.uid=json::num(d,{"uid"});out.status=(int)json::num(d,{"live_status"});
    if(out.room.id<=0){out.error="Invalid live room";return out;}
    if(json::boolean(d,{"is_locked"})){out.error="This live room is unavailable.";return out;}
    if(out.status!=1){out.error=out.status==2?"This room is replaying, not live.":"This room is not live right now.";return out;}
    auto streams=json::at(d,{"playurl_info","playurl","stream"});std::set<std::string> seen;
    for(size_t i=0;i<std::min<size_t>(json::size(streams),12);++i){auto stream=json_array_get(streams,i);auto formats=json::at(stream,{"format"});for(size_t j=0;j<std::min<size_t>(json::size(formats),12);++j){auto f=json_array_get(formats,j);auto format=json::str(f,{"format_name"});if(format!="ts"&&format!="flv"&&format!="fmp4")continue;auto codecs=json::at(f,{"codec"});for(size_t k=0;k<std::min<size_t>(json::size(codecs),12);++k){auto c=json_array_get(codecs,k);if(json::str(c,{"codec_name"})!="avc")continue;auto base=json::str(c,{"base_url"});if(base.empty()||base.front()!='/')continue;auto hosts=json::at(c,{"url_info"});for(size_t n=0;n<std::min<size_t>(json::size(hosts),6);++n){auto host=json_array_get(hosts,n);auto prefix=json::str(host,{"host"});if(prefix.empty())continue;if(prefix.back()=='/')prefix.pop_back();auto u=url(prefix+base+json::str(host,{"extra"}));if(!u.empty()&&seen.insert(u).second&&out.streams.size()<18)out.streams.push_back({u,format,(int)json::num(c,{"current_qn"})});}}}}
    std::stable_sort(out.streams.begin(),out.streams.end(),[](const auto& a,const auto& b){auto rank=[](const auto& x){return (util::starts_with(x.url,"https://")?0:10)+(x.format=="ts"?0:x.format=="flv"?1:2);};return rank(a)<rank(b);});
    out.ok=!out.streams.empty();if(!out.ok)out.error="No compatible H.264 live stream. Sign in or try another room.";return out;
}
Rooms browse(int page,int parent,int area,const std::string& cookie,const std::atomic<bool>* cancel) {
    Rooms r;page=std::clamp(page,1,100);auto body=request("/xlive/app-interface/v2/index/feedV2",{{"parent_area_id",std::to_string(std::max(0,parent))},{"area_id",std::to_string(std::max(0,area))},{"device","switch"},{"page",std::to_string(page)},{"platform","web"},{"scale","xxhdpi"},{"source_name","pc"},{"mobi_app","pc_electron"}},cookie,cancel,r.error);
    return r.error.empty()?parse_rooms(body,page):r;
}
Rooms followed(int page,const std::string& cookie,const std::atomic<bool>* cancel) {
    Rooms r;if(cookie.empty()){r.error="Sign in to see followed live rooms.";return r;}page=std::clamp(page,1,100);
    auto body=request("/xlive/web-ucenter/v1/xfetter/GetWebList",{{"page",std::to_string(page)},{"page_size","20"}},cookie,cancel,r.error);
    if(!r.error.empty())return r;
    auto parsed=parse_rooms(body,page,true);
    if(parsed.ok||parsed.auth_expired||page>1)return parsed;
    r.error.clear();body=request("/xlive/app-interface/v2/index/feedV2",{{"parent_area_id","0"},{"area_id","0"},{"device","switch"},{"page","1"},{"platform","web"},{"scale","xxhdpi"},{"source_name","pc"},{"mobi_app","pc_electron"}},cookie,cancel,r.error);
    return r.error.empty()?parse_rooms(body,1,true):r;
}
Areas areas(const std::atomic<bool>* cancel) {Areas r;auto body=request("/xlive/app-interface/v2/index/getAreaList",{{"platform","web"}},"",cancel,r.error);return r.error.empty()?parse_areas(body):r;}
Playback playback(int64_t id,int qn,const std::string& cookie,const std::atomic<bool>* cancel) {
    Playback r;if(id<=0){r.error="Enter a live room number or live.bilibili.com URL.";return r;}
    auto body=request("/xlive/web-room/v2/index/getRoomPlayInfo",{{"room_id",std::to_string(id)},{"no_playurl","0"},{"mask","1"},{"qn",std::to_string(qn)},{"platform","web"},{"protocol","0,1"},{"format","0,1,2"},{"codec","0"},{"dolby","5"},{"ptype","8"},{"panorama","1"}},cookie,cancel,r.error,false);
    return r.error.empty()?parse_playback(body):r;
}
player::Source make_source(const Room& room,int qn,const std::string& cookie) {
    player::Source s;s.title=room.title.empty()?"Bilibili Live "+std::to_string(room.id):room.title;s.subtitle=room.author;s.artwork=room.picture;
    s.service="bilibili_live";s.id=std::to_string(room.id);s.live=true;s.remember_position=false;s.http_only=true;s.chunked_http=false;s.user_agent=UA;
    s.headers={{"Referer","https://live.bilibili.com/"+s.id}};
    auto attempts=std::make_shared<std::atomic<size_t>>(0);
    s.resolve=[id=room.id,qn,cookie,attempts](player::Source& out,std::string& fail){auto p=playback(id,qn,cookie,out.resolve_cancel);if(!p.ok){fail=tr(p.error.c_str());return false;}
        // Refresh signed URLs every time, and try another secure candidate on restart.
        const size_t secure=std::count_if(p.streams.begin(),p.streams.end(),[](const auto& stream){return util::starts_with(stream.url,"https://");});
        const size_t first=attempts->fetch_add(1) % (secure ? secure : p.streams.size());
        for(size_t i=0;i<p.streams.size();++i){const auto& stream=p.streams[i<secure ? (first+i)%secure : (secure ? i : (first+i)%p.streams.size())];if(out.resolve_cancel&&out.resolve_cancel->load()){fail="Cancelled";return false;}http::Request req;req.url=stream.url;req.headers=out.headers;req.headers.emplace_back("User-Agent",UA);req.cancel=out.resolve_cancel;req.timeout=6;req.stall_seconds=3;req.max_bytes=8192;bool readable=false;req.private_url=true;req.require_tls=util::starts_with(req.url,"https://");req.on_data=[&readable](const http::Response& r,const char*,size_t n){readable=r.ok()&&n>0;return false;};const auto response=http::perform(req);if(readable||(response.ok()&&!response.body.empty())){out.url=stream.url;out.audio_url.clear();fail.clear();return true;}}
        fail=tr("Bilibili live CDN unavailable. Try again.");return false;};return s;
}
}

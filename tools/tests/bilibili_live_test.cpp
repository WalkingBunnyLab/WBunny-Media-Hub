#include "services/bilibili_live.hpp"
#include "core/http.hpp"
#include "core/md5.hpp"
#include "player/player.hpp"
#include <cassert>
#include <cstdio>
#include <deque>
namespace bl=bilibili::live;
static std::deque<http::Response> replies;
static std::vector<http::Request> requests;
namespace http { Response perform(const Request& r) { requests.push_back(r); assert(!replies.empty());auto out=replies.front();replies.pop_front();if(r.on_data&&!out.body.empty())r.on_data(out,out.body.data(),out.body.size());return out; } }
static http::Response reply(std::string body,long status=200){http::Response r;r.body=std::move(body);r.status=status;return r;}
static const char* FEED=R"({"code":0,"data":{"has_more":true,"card_list":[{"card_type":"area_entrance_v1"},{"card_type":"small_card_v1","card_data":{"small_card_v1":{"roomid":6,"title":"<b>Room</b>","uname":"UP","cover":"//image.test/a.jpg","watched_show":{"num":25}}}},{"card_type":"second_card_v1","card_data":{"second_card_v1":{"roomid":6}}},{"card_type":"my_idol_v1","card_data":{"my_idol_v1":{"list":[{"roomid":7,"uname":"Following"}]}}}]}})";
static const char* PLAY=R"({"code":0,"data":{"room_id":7734200,"live_status":1,"playurl_info":{"playurl":{"stream":[{"format":[{"format_name":"flv","codec":[{"codec_name":"hevc","base_url":"/bad.flv","url_info":[{"host":"https://cdn.test/"}]},{"codec_name":"avc","current_qn":150,"base_url":"/live.flv","url_info":[{"host":"http://cdn.test/"},{"host":"https://cdn.test","extra":"?token=x"},{"host":"file:///"}]}]},{"format_name":"ts","codec":[{"codec_name":"avc","current_qn":150,"base_url":"/index.m3u8","url_info":[{"host":"https://cdn.test/"}]}]}]}]}}}})";
static bool cookie(const http::Request& r){for(auto& h:r.headers)if(h.first=="Cookie")return true;return false;}
int main(){
    for(auto s:{"6"," 6 ","https://live.bilibili.com/6?x=1","http://live.bilibili.com/6/"})assert(bl::room_id(s)==6);
    for(auto s:{"0","-6","BV123","https://evil.test/6","https://live.bilibili.com.evil/6","https://live.bilibili.com/@6","9999999999999999"})assert(!bl::room_id(s));
    auto q=bl::signed_query({{"z","a b&"},{"sign","discard"}},123);
    std::string expected="appkey=fixture-key&build=1&ts=123&z=a%20b%26";
    assert(q==expected+"&sign="+md5::hash(expected+"fixture-secret"));
    auto f=bl::parse_rooms(FEED,1);assert(f.ok&&f.items.size()==2&&f.next_page==2&&f.items[0].picture=="https://image.test/a.jpg"&&f.items[0].viewers==25);
    f=bl::parse_rooms(FEED,1,true);assert(f.ok&&f.items.size()==1&&f.items[0].following&&!f.next_page);
    f=bl::parse_rooms(R"({"code":0,"data":{"count":40,"list":[{"room_id":8,"live_status":0},{"room_id":9,"live_status":1}]}})",1,true);assert(f.items.size()==1&&f.items[0].id==9&&f.next_page==2);
    assert(bl::parse_rooms(R"({"code":-101})",1).auth_expired);
    for(auto s:{"broken","{}",R"({"code":"-101"})",R"({"code":0,"data":{}})",R"({"code":-352})"})assert(!bl::parse_rooms(s,1).ok);
    assert(bl::parse_rooms(R"({"code":0,"data":{"card_list":[null,{"card_type":"small_card_v1"}]}})",1).items.empty());
    auto a=bl::parse_areas(R"({"code":0,"data":{"list":[{"id":1,"name":"Games","area_list":[{"id":10,"name":"Console"}]}]}})");assert(a.ok&&a.items.size()==2&&a.items[1].parent==1&&a.items[1].id==10);
    assert(!bl::parse_areas("{}").ok);
    auto p=bl::parse_playback(PLAY);assert(p.ok&&p.room.id==7734200&&p.streams.size()==3&&p.streams[0].format=="ts"&&p.streams[1].url=="https://cdn.test/live.flv?token=x"&&p.streams[2].url.find("http://")==0);
    for(auto s:{R"({"code":0,"data":{"room_id":6,"live_status":0}})",R"({"code":0,"data":{"room_id":6,"live_status":2}})",R"({"code":0,"data":{"room_id":6,"live_status":1,"is_locked":true}})",R"({"code":0,"data":{"room_id":6,"live_status":1}})"})assert(!bl::parse_playback(s).ok);
    std::atomic<bool> cancel{false};replies.push_back(reply(FEED));assert(bl::browse(2,1,10,"SESSDATA=fixture",&cancel).ok);
    auto r=requests.back();assert(r.require_tls&&r.private_url&&cookie(r)&&r.cancel==&cancel&&r.url.find("area_id=10")!=std::string::npos&&r.url.find("sign=")!=std::string::npos);
    assert(!bl::followed(1,"",&cancel).ok);
    replies.push_back(reply(R"({"code":-352})"));replies.push_back(reply(FEED));assert(bl::followed(1,"fake",&cancel).items.size()==1);
    replies.push_back(reply(R"({"code":-101})"));assert(bl::followed(1,"fake",&cancel).auth_expired);
    bl::Room room;room.id=6;auto source=bl::make_source(room,150,"SESSDATA=fixture");assert(source.live&&!source.remember_position&&!source.chunked_http&&source.http_only&&source.service=="bilibili_live");
    source.resolve_cancel=&cancel;std::string error;replies.push_back(reply(PLAY));replies.push_back(reply("",403));replies.push_back(reply("FLV"));
    assert(source.resolve(source,error)&&source.url=="https://cdn.test/live.flv?token=x"&&error.empty());
    assert(cookie(requests[requests.size()-3])&&!cookie(requests[requests.size()-2])&&!cookie(requests.back()));
    assert(requests.back().private_url&&requests.back().require_tls);
    // A restarted Source copies the resolver; its shared attempt count survives.
    auto restarted=source;
    replies.push_back(reply(PLAY));replies.push_back(reply("FLV"));
    assert(restarted.resolve(restarted,error)&&restarted.url=="https://cdn.test/live.flv?token=x");
    assert(cookie(requests[requests.size()-2])&&!cookie(requests.back())&&requests.back().require_tls);
    replies.push_back(reply(PLAY));replies.push_back(reply("TS"));
    assert(restarted.resolve(restarted,error)&&restarted.url=="https://cdn.test/index.m3u8");
    replies.push_back(reply(PLAY));cancel.store(true);assert(!source.resolve(source,error)&&error=="Cancelled");
    assert(replies.empty());puts("PASS live signing, IDs, feeds, areas, status/codec filtering, auth, CDN failover and cookie isolation");
}

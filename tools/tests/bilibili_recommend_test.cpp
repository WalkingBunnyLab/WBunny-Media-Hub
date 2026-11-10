#include "services/bilibili.hpp"
#include "core/http.hpp"
#include <cassert>
#include <cstdio>
#include <deque>
static std::deque<http::Response> replies;
static std::vector<http::Request> requests;
namespace http { Response perform(const Request& r) { requests.push_back(r); assert(!replies.empty()); auto out = replies.front(); replies.pop_front(); return out; } }
static http::Response reply(const std::string& body) { http::Response r; r.status = 200; r.body = body; return r; }
static const char* NAV = R"({"code":0,"data":{"isLogin":true,"mid":123,"wbi_img":{"img_url":"https://i.test/abcdefghijklmnopqrstuvwxyz012345.png","sub_url":"https://i.test/ABCDEFGHIJKLMNOPQRSTUVWXYZ678901.png"}}})";
static const char* FEED = R"({"code":0,"data":{"item":[
 {"goto":"av","bvid":"BV1GJ411x7h7","title":"Test","pic":"//images.test/c.jpg","duration":90,"owner":{"name":"UP"},"stat":{"view":123}},
 {"goto":"av","bvid":"BV1GJ411x7h7","title":"duplicate"},
 {"goto":"live","bvid":"BV1xx411c7mD","title":"live"},
 {"goto":"av","bvid":"BV1xx411c7mD","title":"ad","is_ad":true},
 {"goto":"av","bvid":"BV1xx411c7mD","title":"ad-info","ad_info":{}},
 {"goto":"av","bvid":"BV1xx411c7mD","title":"business-ad","business_info":{"is_ad":true}},
 {"goto":"bangumi","title":"pgc"}]}})";
int main() {
    using namespace bilibili;
    assert(!recommend("", "", 1).ok && requests.empty());
    auto parsed = parse_list(FEED, 1, false, true);
    assert(parsed.ok && parsed.items.size()==1 && parsed.next_page==2 && parsed.items[0].author=="UP");
    assert(parsed.items[0].picture=="https://images.test/c.jpg" && parsed.items[0].views==123);
    assert(parse_list(R"({"code":0,"data":{"item":[{"bvid":"BV1xx411c7mD","title":"video without goto"}]}})",1,false,true).items.size()==1);
    assert(parse_list(R"({"code":0,"data":{"item":[]}})",1,false,true).next_page==0);
    assert(parse_list(R"({"code":0,"data":{"item":[],"no_more":true}})",1,false,true).ok);
    assert(!parse_list(R"({"code":0,"data":{"list":[]}})",1,false,true).ok);
    assert(!parse_list(R"({"code":"-101"})",1,false,true).auth_expired);
    assert(!parse_list(R"({"code":0,"data":{"item":[],"v_voucher":"challenge"}})",1,false,true).ok);
    for (int page : {1,2}) {
        replies.push_back(reply(NAV)); replies.push_back(reply(FEED));
        auto feed = recommend("SESSDATA=fake", "123", page, true);
        assert(feed.ok && feed.next_page==page+1);
        const auto& r = requests.back();
        assert(r.url.find("/wbi/index/top/feed/rcmd?")!=std::string::npos && r.url.find("w_rid=")!=std::string::npos);
        assert(r.url.find("fresh_idx="+std::to_string(page))!=std::string::npos);
        assert(r.url.find(page==1?"fresh_type=3":"fresh_type=4")!=std::string::npos);
    }
    for (const auto& r : requests) {
        assert(r.private_url && r.require_tls && r.url.find("https://api.bilibili.com/")==0);
        bool cookie=false; for (const auto& h:r.headers) if(h.first=="Cookie") cookie=h.second=="SESSDATA=fake";
        assert(cookie);
    }
    replies.push_back(reply(R"({"code":-101})")); assert(recommend("fake","123").auth_expired);
    replies.push_back(reply(R"({"code":0,"data":{"isLogin":false}})")); assert(recommend("fake","123").auth_expired);
    replies.push_back(reply("broken JSON")); auto bad=recommend("fake","123"); assert(!bad.ok && !bad.auth_expired);
    auto network=reply(""); network.error="timeout"; replies.push_back(network);
    assert(!recommend("fake","123").auth_expired);
    replies.push_back(reply(NAV)); assert(!recommend("fake","other-user").auth_expired);
    replies.push_back(reply(NAV)); replies.push_back(reply(R"({"code":-101})"));
    assert(recommend("fake","123").auth_expired);
    assert(replies.empty()); puts("PASS recommendation auth, WBI signing, pagination, filters and expiry/network distinction");
}

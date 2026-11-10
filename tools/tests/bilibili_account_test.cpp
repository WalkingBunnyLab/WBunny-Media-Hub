#include "services/bilibili_account.hpp"
#include "core/http.hpp"
#include <cassert>
#include <cstdio>
#include <deque>

static std::deque<http::Response> replies;
static std::vector<http::Request> requests;
namespace http { Response perform(const Request& r) { requests.push_back(r); assert(!replies.empty()); auto out = replies.front(); replies.pop_front(); return out; } }
static http::Response reply(const std::string& body) { http::Response r; r.status = 200; r.body = body; return r; }
int main() {
    namespace ba = bilibili::account;
    replies.push_back(reply(R"({"code":0,"data":{"url":"https://passport.bilibili.com/h5-app/passport/login/scan?x=test","qrcode_key":"fake-key"}})"));
    auto code = ba::generate(nullptr); assert(code.error.empty() && code.key == "fake-key");
    replies.push_back(reply(R"({"code":0,"data":{"url":"https://account.bilibili.com/h5/account-h5/auth/scan-web?x=test","qrcode_key":"fake-key"}})"));
    assert(ba::generate(nullptr).error.empty());
    replies.push_back(reply(R"({"code":0,"data":{"url":"https://evil.invalid/","qrcode_key":"fake"}})"));
    assert(!ba::generate(nullptr).error.empty());
    for (int state : {86101, 86090, 86038}) {
        replies.push_back(reply("{\"code\":0,\"data\":{\"code\":" + std::to_string(state) + "}}"));
        auto l = ba::poll("fake-key", nullptr); assert(l.error.empty() && l.state == state && !l.session.ok());
    }
    replies.push_back(reply(R"({"code":0,"data":{}})")); assert(!ba::poll("fake", nullptr).error.empty());
    auto success = reply(R"({"code":0,"data":{"code":0}})");
    success.set_cookies = {"SESSDATA=fake-session; Path=/; Secure", "bili_jct=fake-csrf; Path=/", "DedeUserID=123; Path=/", "unrelated=no; Path=/"};
    replies.push_back(success);
    replies.push_back(reply(R"({"code":0,"data":{"isLogin":true,"mid":123,"uname":"test account"}})"));
    auto login = ba::poll("fake-key", nullptr); assert(login.error.empty() && login.session.ok() && login.session.name == "test account");
    assert(login.session.cookie.find("unrelated") == std::string::npos);
    const auto s = login.session;
    replies.push_back(reply(R"({"code":0,"data":{"list":[{"id":456,"title":"folder","fav_state":1}]}})"));
    auto folders = ba::folders(s, 9876543210LL, nullptr); assert(folders.ok && folders.folders.size() == 1 && folders.folders[0].contains);
    replies.push_back(reply(R"({"code":0,"data":{"medias":[{"bvid":"BV1GJ411x7h7","title":"test","cover":"//i0.hdslb.com/test.jpg","upper":{"name":"author"},"duration":30}],"has_more":true}})"));
    auto fav = ba::favorites(s, "456", 1, nullptr); assert(fav.ok && fav.next_page == 2 && fav.videos.size() == 1);
    replies.push_back(reply(R"({"code":0,"data":{"list":[{"title":"watched","history":{"bvid":"BV1GJ411x7h7","business":"archive"},"cover":"https://i0.hdslb.com/test.jpg","author_name":"author"}],"cursor":{"max":9,"view_at":42}}})"));
    auto hist = ba::history(s, "", nullptr); assert(hist.ok && hist.videos.size() == 1 && hist.cursor.find("view_at=42") != std::string::npos);
    replies.push_back(reply(R"({"code":-101,"message":"login"})")); assert(!ba::history(s, "", nullptr).ok);
    replies.push_back(reply(R"({"code":0})")); assert(ba::favorite(s, 9876543210LL, "456", true, nullptr).empty());
    assert(requests.back().method == "POST" && requests.back().body.find("csrf=fake-csrf") != std::string::npos && requests.back().body.find("rid=9876543210") != std::string::npos);
    for (const auto& r : requests) assert(r.require_tls && r.private_url);
    assert(replies.empty()); puts("PASS QR state machine, login validation, favorites, history, CSRF, private TLS requests");
}

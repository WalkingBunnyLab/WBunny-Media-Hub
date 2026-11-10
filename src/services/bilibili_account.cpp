#include "services/bilibili_account.hpp"
#include "core/http.hpp"
#include "core/json.hpp"
#include "core/store.hpp"
#include "core/util.hpp"
#include "player/player.hpp"
#include <algorithm>
#include <map>

namespace bilibili::account {
namespace {
constexpr const char* API = "https://api.bilibili.com";
constexpr const char* PASSPORT = "https://passport.bilibili.com";
http::Response call(const char* host, const std::string& path, const Session& s,
                    const std::atomic<bool>* cancel, const std::string& body = "") {
    http::Request r;
    r.url = std::string(host) + path;
    r.require_tls = true;
    r.private_url = true;
    r.cancel = cancel;
    r.max_bytes = 4 * 1024 * 1024;
    r.headers = {{"Referer", "https://www.bilibili.com/"}, {"User-Agent", "Mozilla/5.0"}};
    if (!s.cookie.empty()) r.headers.emplace_back("Cookie", s.cookie);
    if (!body.empty()) {
        r.method = "POST"; r.body = body;
        r.headers.emplace_back("Content-Type", "application/x-www-form-urlencoded");
    }
    return http::perform(r);
}
std::string error(const http::Response& r, json_t* j) {
    if (!r.ok()) return r.error.empty() ? "Bilibili HTTP " + std::to_string(r.status) : "Bilibili network request failed";
    if (!json_is_integer(json::at(j, {"code"}))) return "Invalid Bilibili response";
    if (json::num(j, {"code"}) == -101) return "Bilibili login expired. Sign in again.";
    if (json::num(j, {"code"})) return "Bilibili: " + json::str(j, {"message"}, "Request failed");
    return "";
}
Video item(json_t* j, bool history) {
    Video v;
    v.bvid = json::str(j, history ? std::initializer_list<json::Key>{"history", "bvid"} : std::initializer_list<json::Key>{"bvid"});
    v.title = json::str(j, {"title"});
    v.picture = json::str(j, {"cover"});
    if (util::starts_with(v.picture, "//")) v.picture = "https:" + v.picture;
    if (!util::starts_with(v.picture, "https://")) v.picture.clear();
    v.author = history ? json::str(j, {"author_name"}) : json::str(j, {"upper", "name"});
    v.aid = json::num(j, {"id"});
    v.duration = (int)json::num(j, {"duration"});
    v.ok = !video_id(v.bvid).empty();
    return v;
}
store::Fav entry(const Video& v) { return {v.bvid, v.title, v.author, v.picture, ""}; }
}
Session session() {
    return {store::get_str("bili_cookie"), store::get_str("bili_csrf"), store::get_str("bili_uid"), store::get_str("bili_name")};
}
void save(const Session& s) {
    if (!s.ok()) return;
    store::set_str("bili_cookie", s.cookie); store::set_str("bili_csrf", s.csrf);
    store::set_str("bili_uid", s.uid); store::set_str("bili_name", s.name);
}
void logout() { for (const char* key : {"bili_cookie", "bili_csrf", "bili_uid", "bili_name"}) store::set_str(key, ""); }
Login generate(const std::atomic<bool>* cancel) {
    Login l;
    auto r = call(PASSPORT, "/x/passport-login/web/qrcode/generate", {}, cancel);
    auto d = json::Doc::parse(r.body); l.error = error(r, d.get());
    if (!l.error.empty()) return l;
    l.url = json::str(d.get(), {"data", "url"}); l.key = json::str(d.get(), {"data", "qrcode_key"});
    const bool trusted = util::starts_with(l.url, "https://passport.bilibili.com/") ||
                         util::starts_with(l.url, "https://account.bilibili.com/");
    if (!trusted || l.key.empty() || l.key.size() > 256)
        { l.error = "Invalid Bilibili login code"; l.url.clear(); l.key.clear(); }
    return l;
}
Login poll(const std::string& key, const std::atomic<bool>* cancel) {
    Login l;
    auto r = call(PASSPORT, "/x/passport-login/web/qrcode/poll?qrcode_key=" + util::url_encode(key), {}, cancel);
    auto d = json::Doc::parse(r.body); l.error = error(r, d.get());
    if (!l.error.empty()) return l;
    if (!json_is_integer(json::at(d.get(), {"data", "code"}))) { l.error = "Invalid login status"; return l; }
    l.state = (int)json::num(d.get(), {"data", "code"});
    if (l.state != 0) return l;
    std::map<std::string, std::string> cookies;
    for (const auto& header : r.set_cookies) {
        auto part = header.substr(0, header.find(';')); auto eq = part.find('=');
        if (eq == std::string::npos || part.find_first_of("\r\n") != std::string::npos) continue;
        const auto name = part.substr(0, eq), value = part.substr(eq + 1);
        if (name == "SESSDATA" || name == "bili_jct" || name == "DedeUserID" || name == "DedeUserID__ckMd5" || name == "sid") cookies[name] = value;
    }
    for (const auto& c : cookies) { if (!l.session.cookie.empty()) l.session.cookie += "; "; l.session.cookie += c.first + "=" + c.second; }
    l.session.csrf = cookies["bili_jct"]; l.session.uid = cookies["DedeUserID"];
    if (cookies["SESSDATA"].empty() || !l.session.ok()) { l.error = "Login did not return a complete session"; return l; }
    auto nav = call(API, "/x/web-interface/nav", l.session, cancel);
    auto n = json::Doc::parse(nav.body); l.error = error(nav, n.get());
    if (l.error.empty() && (!json::boolean(n.get(), {"data", "isLogin"}) || json::str(n.get(), {"data", "mid"}) != l.session.uid))
        l.error = "Bilibili could not verify this login";
    l.session.name = json::str(n.get(), {"data", "uname"});
    return l;
}
Library folders(const Session& s, int64_t aid, const std::atomic<bool>* cancel) {
    Library l; if (!s.ok()) { l.error = "Sign in to Bilibili first"; return l; }
    auto r = call(API, "/x/v3/fav/folder/created/list-all?up_mid=" + util::url_encode(s.uid) + (aid > 0 ? "&type=2&rid=" + std::to_string(aid) : ""), s, cancel);
    auto d = json::Doc::parse(r.body); l.error = error(r, d.get());
    auto list = json::at(d.get(), {"data", "list"});
    if (l.error.empty() && !json_is_array(list) && !json_is_null(list)) { l.error = "Invalid favorites response"; return l; }
    for (size_t i = 0; i < std::min<size_t>(json::size(list), 100); ++i) {
        auto f = json_array_get(list, i); auto id = json::str(f, {"id"});
        if (json::num(f, {"id"}) > 0) l.folders.push_back({id, json::str(f, {"title"}), json::num(f, {"fav_state"}) != 0});
    }
    l.ok = l.error.empty(); return l;
}
Library favorites(const Session& s, const std::string& folder, int page, const std::atomic<bool>* cancel) {
    Library l; if (!s.ok()) { l.error = "Sign in to Bilibili first"; return l; }
    auto r = call(API, "/x/v3/fav/resource/list?media_id=" + util::url_encode(folder) + "&pn=" + std::to_string(page) + "&ps=20&platform=web", s, cancel);
    auto d = json::Doc::parse(r.body); l.error = error(r, d.get());
    auto list = json::at(d.get(), {"data", "medias"});
    if (l.error.empty() && !json_is_array(list) && !json_is_null(list)) l.error = "Invalid favorites response";
    for (size_t i = 0; i < json::size(list); ++i) { auto v = item(json_array_get(list, i), false); if (v.ok) l.videos.push_back(v); }
    l.next_page = json::boolean(d.get(), {"data", "has_more"}) ? page + 1 : 0;
    l.ok = l.error.empty(); return l;
}
Library history(const Session& s, const std::string& cursor, const std::atomic<bool>* cancel) {
    Library l; if (!s.ok()) { l.error = "Sign in to Bilibili first"; return l; }
    auto r = call(API, "/x/web-interface/history/cursor?ps=20&type=archive" + cursor, s, cancel);
    auto d = json::Doc::parse(r.body); l.error = error(r, d.get());
    auto list = json::at(d.get(), {"data", "list"});
    if (l.error.empty() && !json_is_array(list)) l.error = "Invalid history response";
    for (size_t i = 0; i < json::size(list); ++i) { auto v = item(json_array_get(list, i), true); if (v.ok) l.videos.push_back(v); }
    const auto max = json::num(d.get(), {"data", "cursor", "max"}), at = json::num(d.get(), {"data", "cursor", "view_at"});
    if (json::size(list) && max > 0) l.cursor = "&max=" + std::to_string(max) + "&view_at=" + std::to_string(at) + "&business=archive";
    if (l.cursor == cursor) l.cursor.clear();
    l.ok = l.error.empty(); return l;
}
std::string favorite(const Session& s, int64_t aid, const std::string& folder, bool add, const std::atomic<bool>* cancel) {
    if (!s.ok() || aid <= 0 || folder.empty()) return "Invalid favorite request";
    auto r = call(API, "/x/v3/fav/resource/deal", s, cancel, "rid=" + std::to_string(aid) + "&type=2&" + (add ? "add_media_ids=" : "del_media_ids=") + util::url_encode(folder) + "&csrf=" + util::url_encode(s.csrf));
    auto d = json::Doc::parse(r.body); return error(r, d.get());
}
void local_favorite(const Video& v) { store::fav_toggle("bilibili", entry(v)); store::fav_trim("bilibili", 200); }
std::vector<Video> local_library(bool history) {
    std::vector<Video> out;
    for (const auto& f : store::favs(history ? "bilibili_history" : "bilibili")) {
        Video v; v.bvid = f.id; v.title = f.title; v.author = f.subtitle; v.picture = f.image; v.ok = true; out.push_back(v);
    }
    return out;
}
player::Source playback(const Video& v, const Part& p, int height, double start) {
    auto s = make_source(v, p, height, session().cookie);
    s.remember_position = true;
    s.start = start >= 0 ? start : store::resume_position(s.service, s.id);
    // Save history only after playback has progressed; finished videos remain in History.
    const auto profile = store::active_profile();
    auto remember = [v, profile](double pos) {
        if (pos < 1 || store::active_profile() != profile) return;
        store::fav_set("bilibili_history", entry(v), true); store::fav_trim("bilibili_history", 200);
    };
    s.on_progress = [remember](double pos, bool) { remember(pos); };
    s.on_stop = [remember](double pos, bool) { remember(pos); };
    return s;
}
}

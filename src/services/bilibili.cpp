#include "services/bilibili.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <mutex>
#include <set>

#include "core/http.hpp"
#include "core/json.hpp"
#include "core/md5.hpp"
#include "core/util.hpp"
#include "player/player.hpp"

namespace bilibili {
namespace {

constexpr const char* API = "https://api.bilibili.com";
constexpr const char* UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/131.0.0.0 Safari/537.36";
const std::vector<std::pair<std::string, std::string>> HEADERS = {
    {"Referer", "https://www.bilibili.com/"}, {"Origin", "https://www.bilibili.com"}, {"User-Agent", UA}
};

std::mutex key_mutex;
std::string cached_key;
int64_t key_time = 0;
std::mutex visitor_mutex;
std::string visitor_cookie;

std::string api_error(json_t* root) {
    if (!json_is_object(root) || !json_is_integer(json::at(root, {"code"}))) return "Invalid Bilibili response";
    const int64_t code = json::num(root, {"code"});
    if (!code) return "";
    if (code == -412 || code == -352) return "Bilibili blocked this request (" + std::to_string(code) + "). Wait before retrying.";
    if (code == -101) return "Sign in to Bilibili again to access this video.";
    return "Bilibili " + std::to_string(code) + ": " + json::str(root, {"message"}, "Request failed");
}

std::string request(const std::string& path, const std::atomic<bool>* cancel, std::string& error, bool search_client = false, const std::string& cookie = "") {
    http::Request req;
    req.url = (cookie.empty() ? util::env_or("COFFEEFLIX_BILIBILI_API", API) : std::string(API)) + path;
    req.headers = HEADERS;
    if (search_client) {
        // Match the desktop-client API request shape used by the wiliwili adapter.
        // Keep the already-tested browser headers for media and playback APIs.
        req.headers[0].second = "https://www.bilibili.com/client";
        req.headers[2].second = "wiliwili";
    }
    {
        std::lock_guard<std::mutex> lock(visitor_mutex);
        if (!cookie.empty()) req.headers.emplace_back("Cookie", cookie);
        else if (!visitor_cookie.empty()) req.headers.emplace_back("Cookie", visitor_cookie);
    }
    if (!cookie.empty()) { req.require_tls = true; req.private_url = true; }
    req.cancel = cancel;
    req.max_bytes = 4 * 1024 * 1024;
    http::Response response = http::perform(req);
    if (!response.ok()) {
        error = response.error.empty() ? "Bilibili HTTP " + std::to_string(response.status) : response.error;
        return "";
    }
    return response.body;
}

std::string key_from_url(const std::string& url) {
    const size_t slash = url.find_last_of('/'), dot = url.find_last_of('.');
    if (slash == std::string::npos || dot == std::string::npos || dot <= slash) return "";
    return url.substr(slash + 1, dot - slash - 1);
}

std::string signing_key(const std::atomic<bool>* cancel, std::string& error) {
    // Serialize key refreshes; never cache a failed response.
    std::lock_guard<std::mutex> lock(key_mutex);
    const int64_t now = util::unix_time();
    if (!cached_key.empty() && now >= key_time && now - key_time < 3600) return cached_key;
    const std::string body = request("/x/web-interface/nav", cancel, error);
    if (!error.empty()) return "";
    json::Doc doc = json::Doc::parse(body);
    // Anonymous nav can return -101 while still including the signing keys.
    std::string key = mixin_key(key_from_url(json::str(doc.get(), {"data", "wbi_img", "img_url"})),
                               key_from_url(json::str(doc.get(), {"data", "wbi_img", "sub_url"})));
    if (key.empty()) {
        error = api_error(doc.get());
        if (error.empty()) error = "Bilibili did not provide WBI signing keys";
        return "";
    }
    cached_key = key;
    key_time = now;
    return key;
}

bool ensure_visitor(const std::atomic<bool>* cancel, std::string& error) {
    {
        std::lock_guard<std::mutex> lock(visitor_mutex);
        if (!visitor_cookie.empty()) return true;
    }
    const std::string body = request("/x/frontend/finger/spi", cancel, error);
    if (!error.empty()) return false;
    json::Doc doc = json::Doc::parse(body);
    error = api_error(doc.get());
    if (!error.empty()) return false;
    const std::string buvid = json::str(doc.get(), {"data", "b_3"});
    if (buvid.empty() || buvid.find_first_of("\r\n;") != std::string::npos) {
        error = "Bilibili did not provide an anonymous visitor cookie";
        return false;
    }
    std::lock_guard<std::mutex> lock(visitor_mutex);
    visitor_cookie = "buvid3=" + buvid;
    return true;
}

std::string http_url(std::string url) {
    if (util::starts_with(url, "//")) url = "https:" + url;
    // Never pass another protocol from a remote API into FFmpeg.
    if (!util::starts_with(url, "https://") && !util::starts_with(url, "http://")) return "";
    return url;
}

std::vector<std::string> stream_urls(json_t* stream) {
    std::vector<std::string> urls;
    auto add = [&](std::string value) {
        value = http_url(std::move(value));
        if (!value.empty() && std::find(urls.begin(), urls.end(), value) == urls.end()) urls.push_back(std::move(value));
    };
    add(json::str(stream, {"baseUrl"}));
    add(json::str(stream, {"base_url"}));
    for (const char* name : {"backupUrl", "backup_url"}) {
        json_t* backups = json::at(stream, {name});
        for (size_t i = 0; i < json::size(backups) && urls.size() < 3; ++i) add(json::str(json_array_get(backups, i)));
    }
    return urls;
}

std::string reachable_url(const std::vector<std::string>& urls, const std::atomic<bool>* cancel, std::string& error) {
    for (const auto& url : urls) {
        if (cancel && cancel->load()) { error = "Cancelled"; return ""; }
        http::Request req;
        req.url = url;
        req.headers = HEADERS;
        req.headers.emplace_back("Range", "bytes=0-1023");
        req.cancel = cancel;
        req.timeout = 6;
        req.stall_seconds = 3;
        req.max_bytes = 4096;
        // Only retain addresses that can actually serve a bounded media range.
        const auto response = http::perform(req);
        if (response.ok() && response.status == 206 && !response.body.empty()) return url;
        error = response.error.empty() ? "HTTP " + std::to_string(response.status) : response.error;
    }
    return "";
}

}  // namespace

std::string video_id(const std::string& input) {
    const std::string s = util::trim(input);
    size_t start = 0;
    if (util::starts_with(s, "https://www.bilibili.com/video/") || util::starts_with(s, "http://www.bilibili.com/video/") ||
        util::starts_with(s, "https://m.bilibili.com/video/")) {
        start = s.find("/video/") + 7;
    }
    if (s.size() < start + 12 || s.compare(start, 2, "BV")) return "";
    for (size_t i = start + 2; i < start + 12; ++i) {
        const unsigned char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) return "";
    }
    if (s.size() > start + 12 && s[start + 12] != '/' && s[start + 12] != '?' && s[start + 12] != '#') return "";
    return s.substr(start, 12);
}

std::string mixin_key(const std::string& image_key, const std::string& sub_key) {
    // WBI's public protocol permutation, applied to the two nav image names.
    constexpr int order[] = {46,47,18,2,53,8,23,32,15,50,10,31,58,3,45,35,27,43,5,49,33,9,42,19,29,28,14,39,12,38,41,13};
    if (image_key.size() != 32 || sub_key.size() != 32) return "";
    const std::string raw = image_key + sub_key;
    std::string key;
    for (int index : order) key += raw[index];
    return key;
}

std::string signed_query(std::map<std::string, std::string> params, const std::string& key, int64_t timestamp) {
    params.erase("w_rid");
    params["wts"] = std::to_string(timestamp);
    std::string query;
    for (auto& [name, value] : params) {
        value.erase(std::remove_if(value.begin(), value.end(), [](char c) {
            return c == '!' || c == '\'' || c == '(' || c == ')' || c == '*';
        }), value.end());
        if (!query.empty()) query += '&';
        query += util::url_encode(name) + "=" + util::url_encode(value);
    }
    return query + "&w_rid=" + md5::hash(query + key);
}

Video parse_video(const std::string& body) {
    Video video;
    json::Doc doc = json::Doc::parse(body);
    video.error = api_error(doc.get());
    if (!video.error.empty()) return video;
    json_t* data = json::at(doc.get(), {"data"});
    video.bvid = video_id(json::str(data, {"bvid"}));
    video.aid = json::num(data, {"aid"});
    video.title = json::str(data, {"title"});
    video.author = json::str(data, {"owner", "name"});
    video.picture = json::str(data, {"pic"});
    video.description = json::str(data, {"desc"});
    video.duration = (int)json::num(data, {"duration"});
    video.views = json::num(data, {"stat", "view"});
    json_t* pages = json::at(data, {"pages"});
    for (size_t i = 0; i < std::min(json::size(pages), size_t(1000)); ++i) {
        json_t* item = json_array_get(pages, i);
        Part part{json::num(item, {"cid"}), (int)json::num(item, {"page"}), json::str(item, {"part"})};
        if (part.cid > 0) video.parts.push_back(std::move(part));
    }
    video.ok = !video.bvid.empty() && !video.title.empty() && !video.parts.empty();
    if (!video.ok) video.error = "Bilibili returned no playable video parts";
    return video;
}

List parse_list(const std::string& body, int page, bool search_results, bool recommendations) {
    List list;
    list.page = page;
    json::Doc doc = json::Doc::parse(body);
    list.auth_expired = json_is_integer(json::at(doc.get(), {"code"})) && json::num(doc.get(), {"code"}) == -101;
    list.error = api_error(doc.get());
    if (!list.error.empty()) return list;
    json_t* data = json::at(doc.get(), {"data"});
    if (!json_is_object(data)) { list.error = "Bilibili returned no list data"; return list; }
    if (!json::str(data, {"v_voucher"}).empty()) {
        list.error = "Bilibili requires verification. Try later or open a BV ID.";
        return list;
    }
    json_t* items = json::at(data, {recommendations ? "item" : search_results ? "result" : "list"});
    // An empty search can omit result, but a non-array result is a protocol error.
    const bool empty_search = search_results && json_is_integer(json::at(data, {"numResults"})) &&
                              json::num(data, {"numResults"}) == 0;
    if ((!json_is_array(items) && items) || (!items && !empty_search)) {
        list.error = "Bilibili returned an invalid video list";
        return list;
    }
    std::set<std::string> seen;
    for (size_t i = 0; i < std::min(json::size(items), size_t(100)); ++i) {
        json_t* item = json_array_get(items, i);
        // The recommendation feed also contains ads, live rooms and PGC cards.
        const auto kind = json::str(item, {"goto"});
        if (recommendations && ((!kind.empty() && kind != "av") || json::boolean(item, {"is_ad"}) ||
            json::boolean(item, {"business_info", "is_ad"}) || json_is_object(json::at(item, {"ad_info"})))) continue;
        Video video;
        video.bvid = video_id(json::str(item, {"bvid"}));
        video.title = util::html_to_text(json::str(item, {"title"}));
        if (video.bvid.empty() || video.title.empty() || !seen.insert(video.bvid).second) continue;
        video.author = search_results ? json::str(item, {"author"}) : json::str(item, {"owner", "name"});
        video.picture = http_url(json::str(item, {"pic"}));
        video.description = json::str(item, {search_results ? "description" : "desc"});
        video.views = search_results ? json::num(item, {"play"}) : json::num(item, {"stat", "view"});
        if (search_results) {
            for (const auto& component : util::split(json::str(item, {"duration"}), ':')) {
                if (component.empty() || component.size() > 5 || component.find_first_not_of("0123456789 ") != std::string::npos) {
                    video.duration = 0;
                    break;
                }
                if (video.duration > 86400) break;
                video.duration = video.duration * 60 + std::atoi(component.c_str());
            }
        } else video.duration = (int)json::num(item, {"duration"});
        // List cards still need fetch_video() before selecting a part.
        list.items.push_back(std::move(video));
    }
    list.ok = true;
    const bool more = search_results ? page < json::num(data, {"numPages"}, page)
                                     : !json::boolean(data, {"no_more"}, !recommendations);
    if (more && !list.items.empty() && page < 100) list.next_page = page + 1;
    return list;
}

List recommend(const std::string& cookie, const std::string& uid, int page, bool refresh, const std::atomic<bool>* cancel) {
    List list;
    if (cookie.empty() || uid.empty()) { list.error = "Sign in to Bilibili to view recommendations."; return list; }
    page = std::clamp(page, 1, 100);
    // Validate the captured account before accepting a personalized feed. Anonymous
    // recommendations must not silently masquerade as an authenticated response.
    const auto nav = request("/x/web-interface/nav", cancel, list.error, false, cookie);
    if (!list.error.empty()) return list;
    auto doc = json::Doc::parse(nav);
    const auto code = json::at(doc.get(), {"code"});
    const auto logged_in = json::at(doc.get(), {"data", "isLogin"});
    list.auth_expired = json_is_integer(code) && (json::num(code) == -101 ||
        (json::num(code) == 0 && json_is_boolean(logged_in) && !json::boolean(logged_in)));
    if (list.auth_expired) { list.error = "Bilibili login expired. Sign in again."; return list; }
    list.error = api_error(doc.get());
    if (!list.error.empty()) return list;
    if (!json::boolean(logged_in) || json::str(doc.get(), {"data", "mid"}) != uid) {
        list.error = "Could not verify the Bilibili account for recommendations."; return list;
    }
    const auto key = mixin_key(key_from_url(json::str(doc.get(), {"data", "wbi_img", "img_url"})),
                               key_from_url(json::str(doc.get(), {"data", "wbi_img", "sub_url"})));
    if (key.empty()) { list.error = "Bilibili did not provide WBI signing keys"; return list; }
    const auto query = signed_query({{"fresh_idx", std::to_string(page)}, {"ps", "20"},
        {"feed_version", "V1"}, {"fresh_type", page > 1 ? "4" : refresh ? "3" : "0"},
        {"plat", "1"}, {"x_num", "3"}, {"y_num", "3"}}, key, util::unix_time());
    const auto body = request("/x/web-interface/wbi/index/top/feed/rcmd?" + query, cancel, list.error, false, cookie);
    if (!list.error.empty()) return list;
    return parse_list(body, page, false, true);
}

List popular(int page, const std::atomic<bool>* cancel) {
    page = std::clamp(page, 1, 100);
    List list;
    const std::string body = request("/x/web-interface/popular?pn=" + std::to_string(page) + "&ps=20", cancel, list.error);
    if (!list.error.empty()) return list;
    return parse_list(body, page);
}

List search(const std::string& query, int page, const std::atomic<bool>* cancel) {
    List list;
    const std::string term = util::trim(query);
    if (term.empty()) { list.ok = true; return list; }
    if (term.size() > 512) { list.error = "Search text is too long"; return list; }
    page = std::clamp(page, 1, 100);
    if (!ensure_visitor(cancel, list.error)) return list;
    const std::string key = signing_key(cancel, list.error);
    if (key.empty()) return list;
    const std::string params = signed_query({{"search_type", "video"}, {"keyword", term},
        {"page", std::to_string(page)}, {"page_size", "20"}, {"order", "totalrank"}, {"highlight", "1"},
        {"__refresh__", "true"}, {"_extra", ""}, {"context", ""}, {"duration", ""}, {"from_source", ""},
        {"from_spmid", "333.337"}, {"platform", "pc"}, {"device", "mac"}, {"single_column", "0"},
        {"category_id", ""}, {"dynamic_offset", "0"}}, key, util::unix_time());
    const std::string body = request("/x/web-interface/wbi/search/type?" + params, cancel, list.error, true);
    if (!list.error.empty()) return list;
    return parse_list(body, page, true);
}

Streams parse_streams(const std::string& body, int max_height) {
    Streams result;
    json::Doc doc = json::Doc::parse(body);
    result.error = api_error(doc.get());
    if (!result.error.empty()) return result;
    json_t* videos = json::at(doc.get(), {"data", "dash", "video"});
    int video_bandwidth = 0;
    for (size_t i = 0; i < json::size(videos); ++i) {
        json_t* stream = json_array_get(videos, i);
        const int height = (int)json::num(stream, {"height"});
        const int bandwidth = (int)json::num(stream, {"bandwidth"});
        const std::string codec = json::str(stream, {"codecs"});
        const bool avc = json::num(stream, {"codecid"}) == 7 || util::starts_with(codec, "avc1");
        const auto urls = stream_urls(stream);
        if (!avc || height <= 0 || height > max_height || urls.empty()) continue;
        if (height > result.height || (height == result.height && (video_bandwidth == 0 || bandwidth < video_bandwidth))) {
            result.video_urls = urls;
            result.video_url = urls.front();
            result.height = height;
            video_bandwidth = std::max(0, bandwidth);
        }
    }
    json_t* audios = json::at(doc.get(), {"data", "dash", "audio"});
    int audio_bandwidth = 0;
    for (size_t i = 0; i < json::size(audios); ++i) {
        json_t* stream = json_array_get(audios, i);
        const std::string codec = json::str(stream, {"codecs"});
        const auto urls = stream_urls(stream);
        const int bandwidth = (int)json::num(stream, {"bandwidth"});
        if (!util::starts_with(codec, "mp4a") || urls.empty()) continue;
        if (result.audio_url.empty() || bandwidth > audio_bandwidth) {
            result.audio_urls = urls;
            result.audio_url = urls.front();
            audio_bandwidth = std::max(0, bandwidth);
        }
    }
    result.ok = !result.video_url.empty() && !result.audio_url.empty();
    result.bitrate = video_bandwidth + audio_bandwidth;
    if (!result.ok) result.error = "No H.264 + AAC DASH streams at this quality. Try another quality or check your account's access.";
    return result;
}

Video fetch_video(const std::string& input, const std::atomic<bool>* cancel) {
    Video result;
    const std::string bvid = video_id(input);
    if (bvid.empty()) { result.error = "Enter a BV ID or a full bilibili.com/video/BV... URL"; return result; }
    const std::string body = request("/x/web-interface/view?bvid=" + bvid, cancel, result.error);
    if (!result.error.empty()) return result;
    return parse_video(body);
}

player::Source make_source(const Video& video, const Part& part, int max_height, const std::string& cookie) {
    player::Source source;
    source.title = video.title;
    source.subtitle = video.author + " / P" + std::to_string(part.page) + " " + part.title;
    source.artwork = video.picture;
    source.service = "bilibili";
    source.id = video.bvid + ":" + std::to_string(part.cid);
    // Resume/navigation integration is deliberately outside this playback experiment.
    source.remember_position = false;
    source.headers = HEADERS;
    source.user_agent = UA;
    source.chunked_http = true;
    source.http_only = true;
    source.quality = max_height;
    const std::string bvid = video.bvid;
    const int64_t cid = part.cid;
    source.resolve = [bvid, cid, cookie](player::Source& out, std::string& error) {
        error.clear();
        const std::string key = signing_key(out.resolve_cancel, error);
        if (key.empty()) return false;
        const int height = out.quality;
        const int qn = height >= 720 ? 64 : height >= 480 ? 32 : 16;
        const std::string query = signed_query({{"bvid", bvid}, {"cid", std::to_string(cid)},
            {"qn", std::to_string(qn)}, {"fnval", "16"}, {"fnver", "0"}, {"fourk", "0"}}, key, util::unix_time());
        const std::string body = request("/x/player/wbi/playurl?" + query, out.resolve_cancel, error, false, cookie);
        if (!error.empty()) return false;
        Streams streams = parse_streams(body, height);
        if (!streams.ok) { error = streams.error; return false; }
        std::string failure;
        out.url = reachable_url(streams.video_urls, out.resolve_cancel, failure);
        if (out.url.empty()) { error = "Bilibili video CDN unavailable: " + failure; return false; }
        out.audio_url = reachable_url(streams.audio_urls, out.resolve_cancel, failure);
        if (out.audio_url.empty()) { error = "Bilibili audio CDN unavailable: " + failure; return false; }
        out.quality = streams.height;
        out.bitrate = streams.bitrate;
        return true;
    };
    return source;
}

}  // namespace bilibili

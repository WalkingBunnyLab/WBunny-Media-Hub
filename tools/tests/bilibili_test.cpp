// Offline protocol fixtures; optional live API/media check with --live BV...
#include <cassert>
#include <cstdio>
#include <string>

#include "core/http.hpp"
#include "player/player.hpp"
#include "services/bilibili.hpp"

#ifdef BILIBILI_TEST_TRANSPORT
// Only transport is replaced: production parsing, signing, Source resolution,
// URL encoding and MD5 run unchanged. No network or Wii U is needed.
static std::string mock_video, mock_streams, mock_popular, mock_search;
static int mock_requests = 0, mock_media_requests = 0;
namespace http {
Response perform(const Request& request) {
    if (request.url.find("https://video/") == 0 || request.url.find("https://audio/") == 0) {
        ++mock_media_requests;
        Response media;
        assert(request.headers.back().first == "Range" && request.timeout <= 6);
        if (request.url == "https://video/720") { media.status = 403; return media; }
        media.status = 206;
        media.body = "media bytes";
        return media;
    }
    ++mock_requests;
    assert(request.headers.size() >= 3 && request.max_bytes <= 4 * 1024 * 1024);
    Response response;
    response.status = 200;
    if (request.cancel && request.cancel->load()) { response.error = "Cancelled"; return response; }
    if (request.url.find("/x/web-interface/nav") != std::string::npos)
        response.body = R"({"code":-101,"data":{"wbi_img":{
            "img_url":"https://i.test/abcdefghijklmnopqrstuvwxyz012345.png",
            "sub_url":"https://i.test/ABCDEFGHIJKLMNOPQRSTUVWXYZ678901.png"}}})";
    else if (request.url.find("/x/frontend/finger/spi") != std::string::npos)
        response.body = R"({"code":0,"data":{"b_3":"anonymous-test-visitor"}})";
    else if (request.url.find("/x/web-interface/popular?") != std::string::npos)
        response.body = mock_popular;
    else if (request.url.find("/x/web-interface/wbi/search/type?") != std::string::npos) {
        assert(request.url.find("search_type=video") != std::string::npos && request.url.find("w_rid=") != std::string::npos);
        assert(request.headers.back().first == "Cookie" && request.headers.back().second == "buvid3=anonymous-test-visitor");
        response.body = mock_search;
    } else if (request.url.find("/x/player/wbi/playurl?") != std::string::npos) {
        assert(request.url.find("cid=12345678901") != std::string::npos);
        assert(request.url.find("&w_rid=") != std::string::npos);
        assert(request.url.find("fnval=16") != std::string::npos);
        response.body = mock_streams;
    } else {
        assert(request.url.find("/x/web-interface/view?bvid=BV1xx411c7mD") != std::string::npos);
        response.body = mock_video;
    }
    return response;
}
}  // namespace http
#endif

int main(int argc, char** argv) {
    using namespace bilibili;
    assert(video_id(" BV1xx411c7mD ") == "BV1xx411c7mD");
    assert(video_id("https://www.bilibili.com/video/BV1xx411c7mD/?p=2") == "BV1xx411c7mD");
    for (const char* input : {"BV123", "BV1xx411c7mDX", "https://evil.test/BV1xx411c7mD", "BV1xx411c7m!", "av123"})
        assert(video_id(input).empty());
    assert(mixin_key("short", "short").empty());
    assert(mixin_key("abcdefghijklmnopqrstuvwxyz012345", "ABCDEFGHIJKLMNOPQRSTUVWXYZ678901") ==
           "OPscVixApSk56dND1LfRBjKt32oHmGJn");
    const std::string query = signed_query({{"foo", "114"}, {"bar", "514"}, {"test", "你好!()*'"}, {"w_rid", "old"}},
                                          "0123456789abcdef0123456789abcdef", 1702204169);
    assert(query == "bar=514&foo=114&test=%E4%BD%A0%E5%A5%BD&wts=1702204169&w_rid=29f20255eb5a256ed0cc17066e0f41c7");

    const Video video = parse_video(R"({"code":0,"data":{"bvid":"BV1xx411c7mD","title":"Test","owner":{"name":"UP"},
        "pages":[{"cid":12345678901,"page":1,"part":"First"},{"cid":2,"page":2,"part":"Second"},{"cid":0}]}})");
    assert(video.ok && video.parts.size() == 2 && video.parts[0].cid == 12345678901LL);
    assert(!parse_video("<html>blocked</html>").ok);
    assert(!parse_video(R"({"data":{}})").ok);
    assert(!parse_video(R"({"code":0,"data":{"bvid":"BV1xx411c7mD","title":"Empty","pages":[]}})").ok);
    assert(parse_video(R"({"code":-412})").error.find("blocked") != std::string::npos);

    const std::string dash = R"({"code":0,"data":{"dash":{"video":[
        {"height":1080,"codecid":7,"baseUrl":"https://video/1080","bandwidth":4000},
        {"height":720,"codecid":12,"baseUrl":"https://video/hevc","bandwidth":1000},
        {"height":720,"codecid":7,"baseUrl":"https://video/720-heavy","bandwidth":3000},
        {"height":720,"codecid":7,"base_url":"//video/720","backup_url":["file:///bad","https://video/backup"],"bandwidth":2000},
        {"height":480,"codecs":"avc1.64001F","baseUrl":"https://video/480","bandwidth":900},
        {"height":720,"codecid":7,"baseUrl":"file:///bad","bandwidth":1}],
        "audio":[{"codecs":"ec-3","baseUrl":"https://audio/dolby","bandwidth":400},
                 {"codecs":"mp4a.40.2","baseUrl":"https://audio/aac","bandwidth":128}]}}})";
    Streams streams = parse_streams(dash, 720);
    assert(streams.ok && streams.height == 720 && streams.video_url == "https://video/720");
    assert(streams.audio_url == "https://audio/aac" && streams.bitrate == 2128);
    assert(streams.video_urls.size() == 2 && streams.video_urls[1] == "https://video/backup");
    assert(parse_streams(dash, 480).video_url == "https://video/480");
    assert(!parse_streams(dash, 360).ok);
    assert(!parse_streams(R"({"code":0,"data":{"dash":{"video":[],"audio":[]}}})", 720).ok);
    assert(!parse_streams(R"({"code":-101})", 720).ok);

    auto source = make_source(video, video.parts[0], 720);
    assert(source.service == "bilibili" && source.id == "BV1xx411c7mD:12345678901");
    assert(source.resolve && source.http_only && source.chunked_http && !source.remember_position);
    assert(source.headers.size() == 3 && !source.user_agent.empty());
    puts("PASS Bilibili IDs, WBI signing, parts, codec/quality selection, errors and source mapping");

    const std::string popular_fixture = R"({"code":0,"data":{"no_more":false,"list":[
        {"bvid":"BV1xx411c7mD","title":"Video &amp; title","pic":"//images.test/cover.jpg","owner":{"name":"UP"},"duration":123,"stat":{"view":456}},
        {"bvid":"BV1xx411c7mD","title":"Duplicate"},{"bvid":"ad","title":"Ad"}]}})";
    const std::string search_fixture = R"({"code":0,"data":{"page":2,"numPages":3,"result":[
        {"bvid":"BV1xx411c7mD","title":"<em class=\"keyword\">Video</em> &amp; title","author":"UP",
         "duration":"2:03","play":456,"pic":"//images.test/cover.jpg"}]}})";
    const auto popular_list = parse_list(popular_fixture, 1);
    const auto search_list = parse_list(search_fixture, 2, true);
    assert(popular_list.ok && popular_list.items.size() == 1 && popular_list.next_page == 2);
    assert(popular_list.items[0].title == "Video & title" && popular_list.items[0].duration == 123);
    assert(popular_list.items[0].picture == "https://images.test/cover.jpg");
    assert(search_list.ok && search_list.next_page == 3 && search_list.items[0].duration == 123);
    assert(search_list.items[0].title == "Video & title" && search_list.items[0].views == 456);
    assert(parse_list(search_fixture, 3, true).next_page == 0);
    assert(!parse_list(R"({"code":0})", 1, true).ok);
    assert(!parse_list(R"({"code":0,"data":{"list":{}}})", 1).ok);
    assert(parse_list(R"({"code":0,"data":{"numPages":0,"numResults":0}})", 1, true).ok);
    assert(!parse_list(R"({"code":0,"data":{"numPages":0}})", 1, true).ok);
    assert(parse_list(R"({"code":0,"data":{"v_voucher":"verification"}})", 1, true).error.find("verification") != std::string::npos);
    puts("PASS popular/search lists, pagination, HTML titles, durations, cover URLs, duplicates and malformed data");

#ifdef BILIBILI_TEST_TRANSPORT
    mock_video = R"({"code":0,"data":{"bvid":"BV1xx411c7mD","title":"Mock",
        "pages":[{"cid":12345678901,"page":1,"part":"First"}]}})";
    mock_streams = dash;
    assert(fetch_video("BV1xx411c7mD").ok);
    const int before = mock_requests;
    assert(!fetch_video("invalid").ok && mock_requests == before);
    std::string error = "previous failure";
    assert(source.resolve(source, error) && error.empty());
    assert(source.url == "https://video/backup" && source.audio_url == "https://audio/aac");
    assert(mock_media_requests == 3);  // rejected primary, working backup, working audio
    assert(mock_requests == before + 2);  // anonymous nav + signed playurl
    mock_streams = R"({"code":-412})";
    assert(!source.resolve(source, error) && error.find("blocked") != std::string::npos);
    assert(mock_requests == before + 3);  // signing key reused
    std::atomic<bool> cancelled{true};
    source.resolve_cancel = &cancelled;
    assert(!source.resolve(source, error) && error == "Cancelled");
    puts("PASS metadata -> anonymous WBI keys -> signed DASH resolver, CDN fallback, cache and cancellation");
    mock_popular = popular_fixture;
    mock_search = search_fixture;
    assert(popular(1).ok);
    assert(search("test", 2).ok);
    const int after_search = mock_requests;
    assert(search("test", 3).ok && mock_requests == after_search + 1);  // visitor/signing keys cached
    assert(search(" ").ok && mock_requests == after_search + 1);  // empty search is local
    assert(!search(std::string(513, 'x')).ok);
    puts("PASS popular and signed search transport, anonymous visitor setup, cache and empty-query handling");
    (void)argc;
    (void)argv;
#else
    if (argc == 2 && std::string(argv[1]) == "--browse") {
        http::init("content/cacert.pem");
        const auto first = popular();
        if (!first.ok || first.items.empty()) { fprintf(stderr, "Popular: %s\n", first.error.c_str()); return 1; }
        const auto second = popular(2);
        if (!second.ok || second.items.empty()) { fprintf(stderr, "Popular page 2: %s\n", second.error.c_str()); return 1; }
        const auto found = search("Nintendo");
        if (!found.ok || found.items.empty()) { fprintf(stderr, "Search: %s\n", found.error.c_str()); return 1; }
        const auto chinese = search("任天堂");
        if (!chinese.ok || chinese.items.empty()) { fprintf(stderr, "Chinese search: %s\n", chinese.error.c_str()); return 1; }
        const auto details = fetch_video(found.items[0].bvid);
        if (!details.ok) { fprintf(stderr, "Search result detail: %s\n", details.error.c_str()); return 1; }
        printf("PASS live popular pages 1/2, English/Chinese signed search, and search result -> parts (%zu/%zu/%zu/%zu items)\n",
               first.items.size(), second.items.size(), found.items.size(), chinese.items.size());
        http::shutdown();
    }
    if (argc == 3 && std::string(argv[1]) == "--live") {
        http::init("content/cacert.pem");
        const Video live = fetch_video(argv[2]);
        if (!live.ok) { fprintf(stderr, "%s\n", live.error.c_str()); return 1; }
        auto playable = make_source(live, live.parts[0], 720);
        std::string error;
        if (!playable.resolve(playable, error)) { fprintf(stderr, "%s\n", error.c_str()); return 1; }
        // Confirm both media URLs accept a range; do not download an entire video.
        for (const auto& url : {playable.url, playable.audio_url}) {
            http::Request request;
            request.url = url;
            request.headers = playable.headers;
            request.headers.emplace_back("Range", "bytes=0-1023");
            request.max_bytes = 4096;
            const auto response = http::perform(request);
            if (response.status != 206 || response.body.empty() || !response.error.empty()) {
                fprintf(stderr, "Media range check failed: HTTP %ld %s\n", response.status, response.error.c_str());
                return 1;
            }
        }
        printf("PASS live metadata, WBI playurl and video/audio byte ranges (%dp)\n", playable.quality);
        http::shutdown();
    }
#endif
}

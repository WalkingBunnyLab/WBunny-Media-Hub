// Local playback experiment: public ordinary videos, without an account.
#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace player { struct Source; }

namespace bilibili {

struct Part {
    int64_t cid = 0;
    int page = 0;
    std::string title;
};

struct Video {
    std::string bvid, title, author, picture, error;
    std::string description;
    int duration = 0;
    int64_t views = 0, aid = 0;
    std::vector<Part> parts;
    bool ok = false;
};

struct List {
    std::vector<Video> items;
    std::string error;
    int page = 1, next_page = 0;
    bool ok = false;
    bool auth_expired = false;  // only a confirmed account rejection, never a network failure
};

struct Streams {
    std::string video_url, audio_url, error;
    std::vector<std::string> video_urls, audio_urls;
    int height = 0, bitrate = 0;
    bool ok = false;
};

std::string video_id(const std::string& input);
// Protocol helpers also exercised by offline fixtures.
std::string mixin_key(const std::string& image_key, const std::string& sub_key);
std::string signed_query(std::map<std::string, std::string> params, const std::string& key, int64_t timestamp);
Video parse_video(const std::string& body);
List parse_list(const std::string& body, int page, bool search_results = false, bool recommendations = false);
List recommend(const std::string& cookie, const std::string& uid, int page = 1, bool refresh = false,
               const std::atomic<bool>* cancel = nullptr);
List popular(int page = 1, const std::atomic<bool>* cancel = nullptr);
List search(const std::string& query, int page = 1, const std::atomic<bool>* cancel = nullptr);
Streams parse_streams(const std::string& body, int max_height);
Video fetch_video(const std::string& input, const std::atomic<bool>* cancel = nullptr);
player::Source make_source(const Video& video, const Part& part, int max_height, const std::string& cookie = "");

}  // namespace bilibili

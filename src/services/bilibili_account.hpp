#pragma once
#include "services/bilibili.hpp"

namespace bilibili::account {
struct Session { std::string cookie, csrf, uid, name; bool ok() const { return !cookie.empty() && !csrf.empty() && !uid.empty(); } };
struct Login { std::string url, key, error; Session session; int state = -1; };
struct Folder { std::string id, title; bool contains = false; };
struct Library { std::vector<Folder> folders; std::vector<Video> videos; std::string error, cursor; int next_page = 0; bool ok = false; };
// Store access stays on the UI thread; network workers receive immutable snapshots.
Session session();
void save(const Session& value);
void logout();
Login generate(const std::atomic<bool>* cancel);
Login poll(const std::string& key, const std::atomic<bool>* cancel);
Library folders(const Session&, int64_t aid, const std::atomic<bool>* cancel);
Library favorites(const Session&, const std::string& folder, int page, const std::atomic<bool>* cancel);
Library history(const Session&, const std::string& cursor, const std::atomic<bool>* cancel);
std::string favorite(const Session&, int64_t aid, const std::string& folder, bool add, const std::atomic<bool>* cancel);
// Local library and resume integration, independent of the network account.
player::Source playback(const Video&, const Part&, int height, double start = -1);
std::vector<Video> local_library(bool history);
void local_favorite(const Video&);
}

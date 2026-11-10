#pragma once
#include <atomic>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace player { struct Source; }
namespace bilibili::live {
struct Room {
    int64_t id = 0, uid = 0, viewers = 0;
    std::string title, author, picture, area;
    bool following = false;
};
struct Rooms { std::vector<Room> items; std::string error; int next_page = 0; bool ok = false, auth_expired = false; };
struct Area { int id = 0, parent = 0; std::string name, parent_name; };
struct Areas { std::vector<Area> items; std::string error; bool ok = false; };
struct Stream { std::string url, format; int qn = 0; };
struct Playback { Room room; std::vector<Stream> streams; std::string error; bool ok = false, auth_expired = false; int status = 0; };
int64_t room_id(const std::string& input);
std::string signed_query(std::map<std::string,std::string> params, int64_t milliseconds);
Rooms parse_rooms(const std::string&, int page, bool followed = false);
Areas parse_areas(const std::string&);
Playback parse_playback(const std::string&);
Rooms browse(int page, int parent, int area, const std::string& cookie, const std::atomic<bool>* cancel);
Rooms followed(int page, const std::string& cookie, const std::atomic<bool>* cancel);
Areas areas(const std::atomic<bool>* cancel);
Playback playback(int64_t room, int qn, const std::string& cookie, const std::atomic<bool>* cancel);
player::Source make_source(const Room&, int qn, const std::string& cookie);
}

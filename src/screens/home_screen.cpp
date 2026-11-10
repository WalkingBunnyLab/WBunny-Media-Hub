// Home: a hero for whatever is focused, and shelves pulled from every service.
#include <ctime>
#include <cstdlib>

#include "core/i18n.hpp"
#include "core/store.hpp"
#include "core/tasks.hpp"
#include "core/util.hpp"
#include "gfx/anim.hpp"
#include "gfx/images.hpp"
#include "player/player.hpp"
#include "platform/platform.hpp"
#include "app/greeting.hpp"
#include "app/personal.hpp"
#include "screens/screens.hpp"
#include "screens/widgets.hpp"
#include "services/jellyfin.hpp"
#include "services/navidrome.hpp"
#include "services/podcasts.hpp"
#include "services/radio.hpp"
#include "services/twitch.hpp"
#include "services/bilibili.hpp"
#include "screens/bilibili_common.hpp"
#include "ui/ui.hpp"

namespace screens {

namespace {

using namespace ui;

struct HomeItem {
    int icon = ic::MOVIE;
    std::string service, title, subtitle, description, image, hero;
    float progress = -1;
    bool live = false;
    std::string badge;
    std::function<void()> open;
    std::function<void()> more;  // X
};

struct Row {
    std::string title;  // English, also the shelf's id: tr() it where it's shown
    CardShape shape = CARD_WIDE;
    float w = 268;
    std::vector<HomeItem> items;
    bool loading = false;
};

HomeItem from_resume(const store::Resume& r) {
    HomeItem h;
    h.title = r.title;
    h.subtitle = r.subtitle;
    h.progress = r.duration > 0 ? (float)(r.position / r.duration) : 0;
    h.badge = r.duration > 0 ? util::fmt(tr("%s left"), util::format_duration(r.duration - r.position).c_str()) : "";
    if (r.service == "bilibili") {
        h.icon = ic::SMART_DISPLAY;
        h.service = "Bilibili";
        h.image = h.hero = r.image;
        h.open = [r] {
            const auto colon = r.id.find(':');
            const auto cid = colon == std::string::npos ? 0 : strtoll(r.id.c_str() + colon + 1, nullptr, 10);
            app::push(make_bilibili_video(r.id.substr(0, colon), cid, r.position));
        };
    } else if (r.service == "podcast") {
        h.icon = ic::PODCASTS;
        h.service = tr("Podcast");
        h.image = h.hero = r.image;
        h.open = [r] { play_audio(podcasts::source_from_resume(r.id, r.title, r.subtitle, r.image, r.extra)); };
    } else {
        h.icon = r.video ? ic::MOVIE : ic::MUSIC;
        h.service = r.service == "smb" ? tr("Network") : r.service == "dlna" ? tr("Media server") : tr("My Media");
        h.image = h.hero = r.image;
        if (h.image.empty() && r.video && r.service == "local") h.image = h.hero = "thumb://" + r.id;
        h.open = [r] {
            player::Source s;
            s.url = r.id;
            s.title = r.title;
            s.subtitle = r.subtitle;
            s.service = r.service;
            s.id = r.id;
            s.extra = r.extra;
            s.chunked_http = r.service == "dlna" && r.extra != "whole";  // as dlna_screen's source_for
            s.start = r.position;
            if (r.video) play_video(s);
            else play_audio(s);
        };
    }
    return h;
}

HomeItem from_jellyfin(const jellyfin::Item& it) {
    HomeItem h;
    h.icon = ic::VIDEO_LIBRARY;
    h.service = "Jellyfin";
    h.title = it.type == "Episode" ? it.series_name : it.name;
    h.subtitle = it.type == "Episode" ? util::fmt(tr("S%d E%d \xC2\xB7 %s"), it.parent_index, it.index, it.name.c_str()) : it.year;
    h.description = it.overview;
    h.image = jellyfin::thumb(it, 480);
    h.hero = jellyfin::backdrop(it, 1280);
    if (it.position > 0 && it.runtime > 0) {
        h.progress = (float)(it.position / it.runtime);
        h.badge = util::fmt(tr("%s left"), util::format_duration(it.runtime - it.position).c_str());
    }
    jellyfin::Item copy = it;
    h.open = [copy] { play_jellyfin_video(copy); };
    return h;
}

HomeItem from_twitch(const twitch::Stream& s) {
    HomeItem h;
    h.icon = ic::LIVE_TV;
    h.service = "Twitch";
    h.title = s.name;
    h.subtitle = s.game;
    h.description = s.title;
    h.image = h.hero = s.preview;
    h.live = true;
    h.badge = util::fmt(tr("%s watching"), util::format_count(s.viewers).c_str());
    h.open = [s] { play_video(twitch::make_source(s)); };
    return h;
}

HomeItem from_bilibili(const bilibili::Video& video) {
    HomeItem h;
    h.icon = ic::SMART_DISPLAY;
    h.service = "Bilibili";
    h.title = video.title;
    h.subtitle = video.author;
    h.description = video.description;
    h.image = h.hero = video.picture;
    h.badge = video.duration > 0 ? util::format_duration(video.duration) : "";
    h.open = [video] { bili::open(video); };
    return h;
}

HomeItem from_station(const radio::Station& s) {
    HomeItem h;
    h.icon = ic::RADIO;
    h.service = tr("Radio");
    h.title = s.name;
    h.subtitle = s.country;
    h.description = s.tags;
    h.image = h.hero = s.favicon;
    h.live = false;
    h.open = [s] { play_audio(radio::make_source(s)); };
    return h;
}

HomeItem from_navidrome_album(const navidrome::Album& al) {
    HomeItem h;
    h.icon = ic::ALBUM;
    h.service = "Navidrome";
    h.title = al.name;
    h.subtitle = al.artist;
    h.description = al.genre;
    h.image = h.hero = navidrome::cover_art_url(al.cover_art.empty() ? al.id : al.cover_art, 500);
    if (al.song_count > 0) h.badge = util::fmt(tr("%d tracks"), al.song_count);
    h.live = false;
    h.open = [al] {
        tasks::submit(tasks::API, [id = al.id]() -> std::function<void()> {
            navidrome::List l = navidrome::get_album(id);
            if (l.songs.empty()) return nullptr;
            std::vector<player::Source> q;
            for (auto& s : l.songs) q.push_back(navidrome::make_source(s));
            return [q = std::move(q)]() mutable {
                screens::play_audio_queue(std::move(q), 0);
            };
        });
    };
    return h;
}

class HomeScreen : public app::Screen {
public:
    HomeScreen() { refresh(); }
    app::Section section() const override { return app::SEC_HOME; }
    void on_enter() override {
        dirty_ = true;
        if (ui::time() - refreshed_ > 3) refresh();
    }

    void frame() override {
        const Theme& t = theme();
        float x0 = content_x();
        Id g = id("home");
        build_rows();

        // --- hero -------------------------------------------------------------------
        const bool compact = H < 640;  // a phone: less hero, so a row of cards shows under it
        const float hero_h = compact ? 244 : 340;
        const HomeItem* f = focused_item();
        if (f && f->hero != hero_url_) {
            prev_hero_ = hero_url_;
            hero_url_ = f->hero;
            hero_t_ = ui::time();
            set_backdrop(f->hero);
        }
        float ht = anim::smoothstep((float)(ui::time() - hero_t_) / 0.5f);
        draw_hero_image(prev_hero_, 1 - ht, hero_h-20);
        draw_hero_image(hero_url_, ht, hero_h-20);

        float ty = 64;
        {
            const std::string& hello = greeting::current();  // chosen once, so it doesn't change while Home is open
            if (!hello.empty()) text::draw_fit(font::label, x0, ty, 760, hello, t.accent);
        }
        ty += 38;
        if (f) {
            gfx::push_alpha(0.4f + 0.6f * ht);
            float slide = (1 - anim::ease_out_cubic(ht)) * 16;
            const text::Font hero_font{text::BOLD,48};
            auto lines = text::wrap(hero_font,f->title,compact?800:420,compact?1:2);
            for (auto& l : lines) {
                text::draw_shadowed(hero_font,x0-slide,ty,l,t.text);
                ty += text::line_height(hero_font);
            }
            text::draw(font::body,x0,ty+12,tr("Your media, your way."),t.text2);
            gfx::pop_alpha();
        } else {
            text::draw(font::display, x0, ty, "WBunny Media Hub", t.text);
            text::draw(font::body, x0, ty + 64, tr("Your media, your way."), t.text2);
        }

        // --- shelves --------------------------------------------------------------------
        page_.begin(id(g, "page"), hero_h, H - hero_h);
        gfx::push_clip(Rect(0, hero_h - 30, W, H - hero_h + 30));
        float y = page_.y(hero_h);
        for (size_t r = 0; r < rows_.size(); r++) {
            Row& row = rows_[r];
            if (row.items.empty() && !row.loading) continue;
            ShelfSpec s;
            s.title_str = tr(row.title.c_str());
            s.count = (int)row.items.size();
            s.loading = row.loading;
            s.shape = row.shape;
            s.item_w = row.shape==CARD_WIDE?268:row.w;
            s.item = [&row](int i) {
                const HomeItem& h = row.items[i];
                CardInfo c;
                c.image = h.image;
                c.title = h.title;
                c.subtitle = h.subtitle;
                c.progress = h.progress;
                c.badge = h.badge;
                c.live = h.live;
                c.icon = h.icon;
                return c;
            };
            s.on_click = [&row](int i) {
                if (row.items[i].open) row.items[i].open();
            };
            s.on_x = [&row](int i) {
                if (row.items[i].more) row.items[i].more();
            };
            s.on_focus = [this, r](int i) {
                focus_row_ = (int)r;
                focus_col_ = i;
            };
            y += shelf(id(g, row.title), x0, y, s, &page_) + 8;
        }
        if(!rows_.empty())y+=100;
        y += start_tiles(g, x0, y);
        gfx::pop_clip();
        page_.end(y + page_.scroll());
        bool more = focus_row_ >= 0 && focus_row_ < (int)rows_.size() && focus_col_ >= 0 &&
                    focus_col_ < (int)rows_[focus_row_].items.size() && rows_[focus_row_].items[focus_col_].more;
        if (more) hint_bar({{"A", tr("Open")}, {"X", tr("More")}});
        else hint_bar({{"A", tr("Open")}});
    }

private:
    void draw_hero_image(const std::string& url, float a, float bottom) {  // `bottom`: where the picture has faded out
        if (url.empty() || a <= 0.01f) return;
        const images::Image* img = images::get(url, 1280, 720);
        if (!img || !img->ready) return;
        float fa = a * images::fade(img, 0.4f);
        const Rect r(80,0,W-80,bottom);
        gfx::push_clip(r);
        gfx::image_cover(img->tex,img->w,img->h,r,0,gfx::WHITE.alpha(fa),.3f);
        // Use the original Figma alpha mask over dynamic artwork. Its complement
        // paints the opaque page color, preserving the design's exact fade.
        const char* key="wbunny:hero-mask-overlay";
        if(!images::has(key,images::NO_EVICT)) {
            std::string data;
            if(util::read_file(platform::content_dir()+"/ui/hero-mask.png",data)) {
                if(SDL_Surface* s=images::decode(data,0,0,0)) {
                    SDL_Surface* rgba=SDL_ConvertSurfaceFormat(s,SDL_PIXELFORMAT_RGBA32,0);SDL_FreeSurface(s);
                    if(rgba){for(int y=0;y<rgba->h;++y)for(int x=0;x<rgba->w;++x){auto* p=(uint8_t*)rgba->pixels+y*rgba->pitch+x*4;p[0]=17;p[1]=21;p[2]=19;p[3]=255-p[3];}images::put(key,rgba,images::NO_EVICT);}
                }
            }
        }
        if(const auto* mask=images::get(key,0,0,images::NO_EVICT);mask&&mask->ready)gfx::image(mask->tex,Rect(80,0,W-80,356));
        gfx::pop_clip();
    }

    const HomeItem* focused_item() {
        if (focus_row_ >= 0 && focus_row_ < (int)rows_.size() && focus_col_ >= 0 &&
            focus_col_ < (int)rows_[focus_row_].items.size())
            return &rows_[focus_row_].items[focus_col_];
        for (auto& r : rows_)
            if (!r.items.empty()) return &r.items[0];
        return nullptr;
    }

    float start_tiles(Id g, float x0, float y) {
        struct Tile { const char* title; const char* sub; int icon; app::Section s; uint32_t color; };
        const Tile tiles[] = {
            {"Bilibili", tr("Search and watch"), ic::SMART_DISPLAY, app::SEC_BILIBILI, 0x00A1D6},
            {tr("Live"), tr("Bilibili live lobby"), ic::LIVE_TV, app::SEC_BILIBILI_LIVE, 0xFB7299},
            {"Jellyfin", jellyfin::signed_in() ? tr("Your library") : tr("Connect your server"), ic::VIDEO_LIBRARY,
             app::SEC_JELLYFIN, 0x7E57C2},
            {"Navidrome", navidrome::signed_in() ? tr("Your music library") : tr("Connect your server"), ic::LIBRARY_MUSIC,
             app::SEC_NAVIDROME, 0x00ACC1},
            {"Twitch", tr("Live streams"), ic::LIVE_TV, app::SEC_TWITCH, 0x9146FF},
            {tr("Radio"), tr("40,000 stations"), ic::RADIO, app::SEC_RADIO, 0xFB8C00},
            {tr("Podcasts"), tr("Shows and episodes"), ic::PODCASTS, app::SEC_PODCASTS, 0xD81B60},
            {tr("My Media"), tr("SD card and network"), ic::FOLDER, app::SEC_MEDIA, 0x00897B},
        };
        std::vector<Tile> shown;  // the services turned on in Settings > Make it yours
        for (const Tile& tile : tiles)
            if (app::section_visible(tile.s)) shown.push_back(tile);
        if (shown.empty()) return 0;
        ShelfSpec s;
        s.title = tr("Explore");
        s.count = (int)shown.size();
        s.item_w = 220;
        s.item = [&tiles = shown](int i) {
            CardInfo c;
            c.title = tiles[i].title;
            c.subtitle = tiles[i].sub;
            c.icon = tiles[i].icon;
            c.tint = gfx::rgb(tiles[i].color).alpha(0.85f);
            return c;
        };
        s.on_click = [&tiles = shown](int i) { app::open_section(tiles[i].s); };
        return shelf(id(g, "explore"), x0, y, s, &page_) + 20;
    }

    void refresh() {
        refreshed_ = ui::time();
        dirty_ = true;
        scope_.reset();
        // A service turned off in Settings > Make it yours is not asked for anything.
        jf_loading_ = app::section_visible(app::SEC_JELLYFIN) && jellyfin::signed_in();
        nd_loading_ = app::section_visible(app::SEC_NAVIDROME) && navidrome::signed_in();
        tw_loading_ = app::section_visible(app::SEC_TWITCH) && !store::favs("twitch").empty();
        bili_loading_ = app::section_visible(app::SEC_BILIBILI);
        if (jf_loading_) {
            scope_.run<std::pair<jellyfin::List, jellyfin::List>>(
                [] { return std::make_pair(jellyfin::resume(), jellyfin::next_up()); },
                [this](std::pair<jellyfin::List, jellyfin::List> r) {
                    jf_resume_ = std::move(r.first.items);
                    jf_next_ = std::move(r.second.items);
                    jf_loading_ = false;
                    dirty_ = true;
                });
        }
        if (nd_loading_) {
            scope_.run<navidrome::List>([] { return navidrome::get_album_list("recent", 12); },
                                        [this](navidrome::List l) {
                nd_albums_ = std::move(l.albums);
                nd_loading_ = false;
                dirty_ = true;
            });
        }
        if (tw_loading_) {
            scope_.run<twitch::Streams>([] { return twitch::followed(); }, [this](twitch::Streams s) {
                tw_live_.clear();
                for (auto& st : s.items)
                    if (st.live) tw_live_.push_back(st);
                tw_loading_ = false;
                dirty_ = true;
            });
        }
        if (bili_loading_)
            scope_.run<bilibili::List>([] { return bilibili::popular(); }, [this](bilibili::List result) {
                bili_ = std::move(result.items);
                bili_loading_ = false;
                dirty_ = true;
            });
    }

    void build_rows() {
        // Only when something changed: favorites/resume lists come from the store.
        if (!dirty_) return;
        dirty_ = false;
        rows_.clear();
        Row cont{N_("Continue watching"), CARD_WIDE, 268, {}, false};
        const bool jf_on = app::section_visible(app::SEC_JELLYFIN), nd_on = app::section_visible(app::SEC_NAVIDROME),
                   tw_on = app::section_visible(app::SEC_TWITCH), radio_on = app::section_visible(app::SEC_RADIO),
                   bili_on = app::section_visible(app::SEC_BILIBILI);
        // What was started in a service that is turned off stays out of Continue watching too.
        auto shown = [](const std::string& service) {
            if (service == "youtube") return false;  // old service history is not Bilibili content
            const app::Section s = personal::section_of_service(service);
            return s == app::SEC_NONE || app::section_visible(s);
        };
        if (jf_on)
            for (auto& jr : jf_resume_) {
                HomeItem h = from_jellyfin(jr);
                h.more = [this, jr] {
                    show_menu(h_title(jr), "", {{tr("Remove from Continue watching"), ic::REMOVE, [this, jr] {
                                                     jellyfin::hide_from_resume(jr);
                                                     jf_resume_.erase(std::remove_if(jf_resume_.begin(), jf_resume_.end(),
                                                                                     [&](const jellyfin::Item& x) { return x.id == jr.id; }),
                                                                      jf_resume_.end());
                                                     removed();
                                                 }}});
                };
                cont.items.push_back(std::move(h));
            }
        std::vector<store::Resume> recent;
        for (auto& r : store::resume_list(60)) if (r.service != "movie" && shown(r.service)) recent.push_back(r);
        std::stable_sort(recent.begin(), recent.end(), [](const auto& a, const auto& b) { return a.updated > b.updated; });
        for (size_t i = 0; i < std::min<size_t>(12, recent.size()); ++i) {
            HomeItem h = from_resume(recent[i]);
            add_remove(h, recent[i]);
            cont.items.push_back(std::move(h));
        }
        cont.loading = cont.items.empty() && jf_loading_;
        rows_.push_back(std::move(cont));

        Row next{N_("Next up"), CARD_WIDE, 268, {}, false};
        if (jf_on) for (auto& it : jf_next_) next.items.push_back(from_jellyfin(it));
        rows_.push_back(std::move(next));

        if (nd_on && (!nd_albums_.empty() || nd_loading_)) {
            Row nd_row{N_("Recently added to Navidrome"), CARD_SQUARE, 180, {}, nd_loading_};
            for (auto& al : nd_albums_) nd_row.items.push_back(from_navidrome_album(al));
            rows_.push_back(std::move(nd_row));
        }

        Row live{N_("Live on Twitch"), CARD_WIDE, 300, {}, tw_loading_};
        if (tw_on) for (auto& s : tw_live_) live.items.push_back(from_twitch(s));
        rows_.push_back(std::move(live));

        Row stations{N_("Your radio stations"), CARD_SQUARE, 160, {}, false};
        if (radio_on) for (auto& s : radio::favorites()) stations.items.push_back(from_station(s));
        rows_.push_back(std::move(stations));

        Row bili{N_("Popular on Bilibili"), CARD_WIDE, 300, {}, bili_loading_};
        if (bili_on) for (auto& v : bili_) bili.items.push_back(from_bilibili(v));
        rows_.push_back(std::move(bili));
    }

    // --- taking things off Continue watching (X on a card)
    void removed() {
        dirty_ = true;
        toast(tr("Removed from Continue watching"), ic::CHECK_CIRCLE);
    }

    static std::string h_title(const jellyfin::Item& it) { return it.type == "Episode" ? it.series_name : it.name; }

    void add_remove(HomeItem& h, const store::Resume& r) {
        MenuItem remove{tr("Remove from Continue watching"), ic::REMOVE, [this, r] {
                            store::resume_remove(r.service, r.id);
                            removed();
                        }};
        std::vector<MenuItem> items;
        items.push_back(remove);
        if (r.position > 0 && r.service != "movie") {
            HomeItem from_start = from_resume([&] { store::Resume c = r; c.position = 0; return c; }());
            items.push_back({tr("Play from the start"), ic::REPLAY_10, from_start.open});
        }
        h.more = [title = r.title, items] { show_menu(title, "", items); };
    }

    std::vector<Row> rows_;
    std::vector<jellyfin::Item> jf_resume_, jf_next_;
    std::vector<navidrome::Album> nd_albums_;
    std::vector<twitch::Stream> tw_live_;
    std::vector<bilibili::Video> bili_;
    bool jf_loading_ = false, nd_loading_ = false, tw_loading_ = false, bili_loading_ = false;
    bool dirty_ = true;
    int focus_row_ = -1, focus_col_ = -1;
    std::string hero_url_, prev_hero_;
    double hero_t_ = -10, refreshed_ = 0;
    tasks::Scope scope_;
    Page page_;
};

}  // namespace

std::unique_ptr<app::Screen> make_home() { return std::make_unique<HomeScreen>(); }

}  // namespace screens

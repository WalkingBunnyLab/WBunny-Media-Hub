// Bilibili browsing and detail pages share CoffeeFlix's existing widgets/player.
#include <algorithm>

#include "core/i18n.hpp"
#include "core/store.hpp"
#include "core/tasks.hpp"
#include "core/util.hpp"
#include "player/player.hpp"
#include "screens/screens.hpp"
#include "screens/widgets.hpp"
#include "screens/bilibili_common.hpp"
#include "screens/bilibili_live_screen.hpp"
#include "services/bilibili.hpp"
#include "services/bilibili_account.hpp"

namespace screens {
namespace {
using namespace ui;

class BilibiliTestScreen : public app::Screen {
public:
    explicit BilibiliTestScreen(std::string bvid = "", bool test = true, int64_t cid = 0, double start = -1) : test_(test), resume_cid_(cid), resume_start_(start) {
        height_ = (int)store::get_int("bilibili_quality", 720);
        if (height_ != 360 && height_ != 480 && height_ != 720) height_ = 720;
        if (!bvid.empty() && !store::kids_profile()) load(bvid);
    }
    app::Section section() const override { return test_ ? app::SEC_SETTINGS : app::SEC_BILIBILI; }

    void frame() override {
        const Theme& t = theme();
        const float x = content_x(), w = W - x - 40;
        const Id group = id("bilibili_test");
        page_.begin(id(group, "page"));
        float y = page_.y(52);
        text::draw(font::display, x, y, test_ ? "Bilibili playback test" : "Bilibili", t.text);
        y += 76;
        if (store::kids_profile()) {
            empty_state(Rect(x, y, w, 260), ic::LOCK, "Bilibili", tr("Switch profile to browse Bilibili."));
            page_.end(y + page_.scroll() + 280);
            hint_bar({{"B", tr("Back")}});
            return;
        }
        auto row = [&](const char* key, const char* label, const char* detail, int icon, int flags = 0) {
            const Id item = id(group, key);
            const bool hit = value_row(item, Rect(x, y, w, 64), label, detail, icon, group, flags);
            if (focused() == item) page_.focus_range(y + page_.scroll() - 20, y + page_.scroll() + 84);
            y += 74;
            return hit;
        };
        if (row("input", tr("Open BV ID / video URL"), input_.empty() ? "BV..." : input_.c_str(), ic::SEARCH, test_ ? F_DEFAULT : 0)) {
            const std::weak_ptr<std::atomic<bool>> alive = scope_.token();
            prompt_text(tr("Open BV ID / video URL"), input_, "BV...", [this, alive](std::string input) {
                auto token = alive.lock();
                if (!token || !token->load()) return;
                load(input);
            });
        }
        const std::string quality = util::fmt(tr("%dp maximum (H.264)"), height_);
        if (row("quality", tr("Bilibili quality"), quality.c_str(), ic::SETTINGS)) {
            height_ = height_ == 720 ? 480 : height_ == 480 ? 360 : 720;
            store::set_int("bilibili_quality", height_);
        }
        if (!input_.empty() && !loading_ && row("retry", tr("Reload video details"), nullptr, ic::REFRESH)) load(input_);

        if (loading_) {
            loading_indicator(x + w / 2, y + 80, tr("Loading Bilibili video..."));
            y += 180;
        } else if (!video_.error.empty()) {
            empty_state(Rect(x, y, w, 230), ic::INFO, tr("Could not load video"), video_.error.c_str());
            y += 240;
        } else if (video_.ok) {
            if (row("favorite", store::fav_has("bilibili", video_.bvid) ? tr("Remove local favorite") : tr("Add local favorite"), nullptr, ic::FAVORITE)) {
                bilibili::account::local_favorite(video_);
            }
            if (row("account_favorite", tr("Account favorites"), account_error_.empty() ? nullptr : account_error_.c_str(), ic::FOLDER) && !account_busy_) {
                const auto session = bilibili::account::session();
                if (!session.ok()) app::push(make_bilibili_account());
                else {
                    account_busy_ = true;
                    auto cancel = cancel_; auto aid = video_.aid;
                    account_scope_.run<bilibili::account::Library>([session, cancel, aid] {
                        return bilibili::account::folders(session, aid, cancel.get());
                    }, [this, session, aid](bilibili::account::Library result) {
                        account_busy_ = false; account_error_ = result.error;
                        if (!result.ok || bilibili::account::session().cookie != session.cookie) return;
                        std::vector<MenuItem> menu;
                        const std::weak_ptr<std::atomic<bool>> alive = account_scope_.token();
                        for (const auto& folder : result.folders) menu.push_back({(folder.contains ? tr("Remove from: ") : tr("Add to: ")) + folder.title, ic::FOLDER, [this, alive, session, aid, folder] {
                            auto token = alive.lock(); if (!token || !token->load()) return;
                            if (bilibili::account::session().cookie != session.cookie || account_busy_) return;
                            account_busy_ = true; auto cancel = cancel_;
                            account_scope_.run<std::string>([session, aid, folder, cancel] {
                                return bilibili::account::favorite(session, aid, folder.id, !folder.contains, cancel.get());
                            }, [this](std::string error) { account_busy_ = false; account_error_ = error; toast(error.empty() ? tr("Favorites updated") : error.c_str(), ic::INFO); });
                        }});
                        if (menu.empty()) account_error_ = tr("Create a favorites folder in Bilibili first.");
                        else show_menu(tr("Account favorites"), "", std::move(menu));
                    });
                }
            }
            text::draw_fit(font::title, x, y, w, video_.title, t.text);
            text::draw_fit(font::body, x, y + 42, w, video_.author + " / " + video_.bvid, t.text2);
            y += 92;
            for (size_t i = 0; i < video_.parts.size(); ++i) {
                const auto& part = video_.parts[i];
                const std::string label = "P" + std::to_string(part.page) + "  " + part.title;
                const std::string key = "part_" + std::to_string(i);
                if (row(key.c_str(), label.c_str(), tr("Play"), ic::PLAY_CIRCLE, !test_ && (resume_cid_ ? part.cid == resume_cid_ : i == 0) ? F_DEFAULT : 0)) {
                    play_video(bilibili::account::playback(video_, part, height_, part.cid == resume_cid_ ? resume_start_ : -1));
                    break;
                }
            }
        } else {
            empty_state(Rect(x, y, w, 210), ic::SMART_DISPLAY, "Bilibili prototype",
                        tr("Open a BV ID or video URL to play."));
            y += 220;
        }
        page_.end(y + page_.scroll() + 60);
        hint_bar({{"A", tr("Select")}, {"B", tr("Back")}});
    }

private:
    void load(const std::string& input) {
        scope_.reset();
        account_scope_.reset(); account_busy_ = false; account_error_.clear();
        input_ = util::trim(input);
        loading_ = true;
        video_ = {};
        const std::string value = input_;
        // Scope cancels result delivery; the separate flag cancels running HTTP too.
        if (cancel_) cancel_->store(true);
        cancel_ = std::make_shared<std::atomic<bool>>(false);
        const auto cancel = cancel_;
        scope_.run<bilibili::Video>([value, cancel] { return bilibili::fetch_video(value, cancel.get()); },
            [this](bilibili::Video video) {
                video_ = std::move(video);
                loading_ = false;
                if (video_.ok) store::set_str("bilibili_test_bvid", video_.bvid);
                if (!test_) ui::reset_focus();
            });
    }

public:
    ~BilibiliTestScreen() override { if (cancel_) cancel_->store(true); }

private:
    Page page_;
    std::string input_ = store::get_str("bilibili_test_bvid");
    bilibili::Video video_;
    bool loading_ = false;
    bool test_ = true;
    int height_ = 720;
    int64_t resume_cid_ = 0;
    double resume_start_ = -1;
    std::shared_ptr<std::atomic<bool>> cancel_;
    tasks::Scope scope_;
    tasks::Scope account_scope_;
    bool account_busy_ = false;
    std::string account_error_;
};

class BilibiliBrowseScreen : public app::Screen {
public:
    explicit BilibiliBrowseScreen(std::string query, bool live = false) : query_(std::move(query)), live_(live) {
        account_ = bilibili::account::session();
        recommended_ = query_.empty() && account_.ok();
        if (!store::kids_profile()) { if (live_) live_view_.activate(); else load(1); }
    }
    void on_enter() override { sync_account(); if (live_) live_view_.on_enter(); }
    ~BilibiliBrowseScreen() override { cancel_->store(true); }
    app::Section section() const override { return store::kids_profile() ? app::SEC_HOME : live_ ? app::SEC_BILIBILI_LIVE : app::SEC_BILIBILI; }

    void frame() override {
        sync_account();
        const Theme& t = theme();
        const float x = content_x(), w = W - x - 40;
        const Id group = id("bilibili_browse");
        const Id top = id(group, "top");
        page_.begin(id(group, "page"));
        float y = page_.y(52);
        text::draw(text::Font{text::BOLD,48}, x, y, live_ ? tr("Live") : "Bilibili", t.text);
        if(!live_&&!store::kids_profile()) {
            float aw=measure_button(tr("Bilibili account"),ic::PERSON);
            if(button(id(top,"account"),Rect(x+w-aw,y+12,aw,40),tr("Bilibili account"),ic::PERSON,BTN_NORMAL,top))app::push(make_bilibili_account());
        }
        y += 84;
        if (store::kids_profile()) {
            empty_state(Rect(x, y, w, 260), ic::LOCK, "Bilibili", tr("Switch profile to browse Bilibili."));
            page_.end(y + page_.scroll() + 280);
            hint_bar({{"B", tr("Back")}});
            return;
        }
        if (search_bar(id(top, "search"), Rect(x, y, w, 48), query_, live_ ? tr("Open live room / URL") : tr("Search Bilibili"), top, F_DEFAULT)) {
            if (live_) live_view_.open_room();
            else prompt_text(tr("Search Bilibili"), query_, tr("Search videos"), [](std::string query) {
                query = util::trim(query);
                if (query.empty()) return;
                const std::string bvid = bilibili::video_id(query);
                if (!bvid.empty()) app::push(make_bilibili_video(bvid));
                else {
                    store::add_recent_search("bilibili", query);
                    app::push(make_bilibili_search(query));
                }
            });
        }
        y += 68;
        float cx = x;
        auto action = [&](const char* key, const char* label, int icon, bool selected = false) {
            const float width = measure_button(label, icon);
            if (cx + width > x + w && cx > x) { cx = x; y += 58; }
            const bool hit = button(id(top, key), Rect(cx, y, width, 48), label, icon, selected ? BTN_TAB : BTN_NORMAL, top);
            cx += width + 12;
            return hit;
        };
        if (!live_ && account_.ok() && action("recommend", tr("Recommendations"), ic::STAR, query_.empty() && recommended_) &&
            (live_ || !query_.empty() || !recommended_)) select_feed(true);
        if (!live_ && action("popular", tr("Popular"), ic::TRENDING, query_.empty() && !recommended_) &&
            (live_ || !query_.empty() || recommended_)) select_feed(false);
        if (action("bv", live_ ? tr("Open live room") : tr("Open BV ID / video URL"), live_ ? ic::LIVE_TV : ic::LINK)) {
            if (live_) live_view_.open_room();
            else prompt_text(tr("Open BV ID / video URL"), "", "BV...", [](std::string value) {
                if (!value.empty()) app::push(make_bilibili_video(value));
            });
        }
        if(!live_&&action("favorites",tr("Favorites"),ic::FAVORITE))app::push(make_bilibili_library(false));
        if(!live_&&action("history",tr("History"),ic::HISTORY))app::push(make_bilibili_library(true));
        if (action("refresh", tr("Refresh"), ic::REFRESH)) { if (live_) live_view_.refresh(); else if (!loading_) load(1, true); }
        y += 68;
        if (live_) {
            if (focus_in_group(top)) page_.focus_range(0, y + page_.scroll());
            y += live_view_.frame(x, y, w, page_, group);
            page_.end(y + page_.scroll() + 50);
            hint_bar({{"A", tr("Play")}, {"X", tr("Save / remove room")}, {"B", tr("Back")}});
            return;
        }
        if (focus_in_group(top)) page_.focus_range(0, y + page_.scroll());
        text::draw(font::title, x, y, !query_.empty() ? tr("Search results") : recommended_ ? tr("Recommendations") : tr("Popular"), t.text);
        y += 50;

        if (items_.empty() && !loading_) {
            if (error_.empty()) empty_state(Rect(x, y, w, 240), ic::SEARCH, tr("No results"), tr("Try different words."));
            else if (empty_state_action(id(group, "retry"), Rect(x, y, w, 240), ic::WIFI_OFF,
                                        tr("Couldn't reach Bilibili"), error_.c_str())) load(1);
            y += 260;
        } else {
            GridSpec grid_spec;
            grid_spec.count = (int)items_.size();
            grid_spec.cols = 4;
            grid_spec.item_w = 268;
            grid_spec.gap_x = 16;
            grid_spec.loading = loading_;
            grid_spec.item = [this](int i) { return bili::video_card(items_[i]); };
            grid_spec.on_click = [this](int i) { bili::open(items_[i]); };
            grid_spec.on_focus = [this](int i) { set_backdrop(items_[i].picture); };
            y += grid(id(group, "videos"), x, y, grid_spec, &page_);
            if (!loading_ && next_page_ && items_.size() < 200) {
                const Id more = id(group, "more");
                if (button(more, Rect(x, y + 12, w, 54), error_.empty() ? tr("Load more") : tr("Try again"),
                           ic::REFRESH, BTN_NORMAL, group)) load(next_page_);
                if (focused() == more) page_.focus_range(y + page_.scroll(), y + page_.scroll() + 90);
                y += 88;
            }
            if (!error_.empty()) y += text::draw_wrapped(font::body, Rect(x, y, w, 120), error_, t.text2, 3) + 20;
        }
        page_.end(y + page_.scroll() + 50);
        hint_bar({{"A", tr("Open")}, {"B", tr("Back")}});
    }

private:
    void sync_account() {
        const auto current = bilibili::account::session();
        if (current.cookie == account_.cookie && current.uid == account_.uid) return;
        account_ = current;
        if (!store::kids_profile()) { if (live_) live_view_.on_enter(); else select_feed(account_.ok()); }
    }
    void select_feed(bool recommended) {
        live_ = false; live_view_.deactivate();
        scope_.reset(); cancel_->store(true);
        cancel_ = std::make_shared<std::atomic<bool>>(false);
        loading_ = false; query_.clear(); recommended_ = recommended && account_.ok();
        page_ = Page(); ui::reset_focus(); load(1);
    }
    void load(int page, bool refresh = false) {
        if (loading_) return;
        if (page == 1) { items_.clear(); next_page_ = 0; }
        error_.clear();
        loading_ = true;
        const std::string query = query_;
        const auto cancel = cancel_;
        const auto account = account_;
        const bool recommendation = query.empty() && recommended_;
        scope_.run<bilibili::List>([query, page, cancel, account, recommendation, refresh] {
            if (recommendation) return bilibili::recommend(account.cookie, account.uid, page, refresh, cancel.get());
            return query.empty() ? bilibili::popular(page, cancel.get()) : bilibili::search(query, page, cancel.get());
        }, [this, page, account, recommendation](bilibili::List result) {
            const auto current = bilibili::account::session();
            if (current.cookie != account.cookie || current.uid != account.uid) { sync_account(); return; }
            if (recommendation && result.auth_expired) {
                bilibili::account::logout();
                toast(tr("Bilibili login expired. Showing popular videos."), ic::INFO);
                sync_account(); return;
            }
            loading_ = false;
            error_ = result.error;
            if (!result.ok) return;
            const size_t before = items_.size();
            for (auto& video : result.items) {
                if (items_.size() >= 200) break;
                if (std::none_of(items_.begin(), items_.end(), [&](const auto& old) { return old.bvid == video.bvid; }))
                    items_.push_back(std::move(video));
            }
            next_page_ = page == 1 || items_.size() > before ? result.next_page : 0;
        });
    }

    std::string query_, error_;
    std::vector<bilibili::Video> items_;
    int next_page_ = 0;
    bool loading_ = false;
    bool recommended_ = false;
    bool live_ = false;
    bili::LiveView live_view_;
    bilibili::account::Session account_;
    std::shared_ptr<std::atomic<bool>> cancel_ = std::make_shared<std::atomic<bool>>(false);
    tasks::Scope scope_;
    Page page_;
};
}  // namespace

std::unique_ptr<app::Screen> make_bilibili_test() { return std::make_unique<BilibiliTestScreen>(); }
std::unique_ptr<app::Screen> make_bilibili() { return std::make_unique<BilibiliBrowseScreen>(""); }
std::unique_ptr<app::Screen> make_bilibili_live() { return std::make_unique<BilibiliBrowseScreen>("", true); }
std::unique_ptr<app::Screen> make_bilibili_search(const std::string& query) { return std::make_unique<BilibiliBrowseScreen>(query); }
std::unique_ptr<app::Screen> make_bilibili_video(const std::string& bvid, int64_t cid, double start) { return std::make_unique<BilibiliTestScreen>(bvid, false, cid, start); }

namespace bili {
CardInfo video_card(const bilibili::Video& video) {
    CardInfo card;
    card.title = video.title;
    card.subtitle = video.author;
    if (video.views > 0) card.subtitle += " / " + util::format_count(video.views);
    card.image = video.picture;
    if (video.duration > 0) card.badge = util::format_duration(video.duration);
    card.icon = ic::SMART_DISPLAY;
    return card;
}
void open(const bilibili::Video& video) { app::push(make_bilibili_video(video.bvid)); }
}  // namespace bili
}  // namespace screens

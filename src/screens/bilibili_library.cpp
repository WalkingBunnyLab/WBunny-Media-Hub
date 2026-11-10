#include "screens/screens.hpp"
#include "screens/widgets.hpp"
#include "screens/bilibili_common.hpp"
#include "services/bilibili_account.hpp"
#include "core/i18n.hpp"
#include "core/qr.hpp"
#include "core/store.hpp"
#include "core/tasks.hpp"
#include <algorithm>

namespace screens {
namespace {
using namespace ui;
namespace ba = bilibili::account;
class AccountScreen : public app::Screen {
public:
    AccountScreen() : profile_(store::active_profile()) { if (!ba::session().ok() && !store::kids_profile()) request(); }
    ~AccountScreen() override { cancel_->store(true); }
    app::Section section() const override { return app::SEC_BILIBILI; }
    void frame() override {
        const auto& t = theme(); const float x = content_x(), w = W - x - 40; const auto g = id("bili_account");
        text::draw(font::display, x, 52, tr("Bilibili account"), t.text);
        if (store::kids_profile() || store::active_profile() != profile_) return;
        const auto s = ba::session();
        if (s.ok()) {
            text::draw(font::title, x, 176, s.name.empty() ? s.uid : s.name, t.text);
            text::draw(font::body, x, 226, "UID " + s.uid, t.text2);
            if (button(id(g, "out"), Rect(x, 300, 260, 64), tr("Sign out"), ic::LOGOUT, BTN_PRIMARY, g, F_DEFAULT)) {
                ba::logout(); request(); return;
            }
            text::draw_wrapped(font::body, Rect(x, 396, w, 100), tr("Account favorites and history are available in Bilibili. Signing out removes the saved login on this profile."), t.text2, 3);
        } else {
            if (qr_.size) qr_code(qr_, x, 146, 280);
            text::draw_wrapped(font::title, Rect(x + 304, 176, w - 304, 90), tr("Scan with the Bilibili mobile app, then confirm sign-in on your phone."), t.text, 2);
            if(!expired_)spinner(x+316,260,12,t.text2,2);
            text::draw_wrapped(font::body, Rect(x + 342, 250, w - 342, 110), message_, t.text2, 4);
            if (button(id(g, "new"), Rect(x, 466, 280, 64), tr("Refresh QR code"), ic::REFRESH, BTN_PRIMARY, g, F_DEFAULT) && !busy_) { request(); return; }
            if (!busy_ && !code_.key.empty() && !expired_ && ui::time() >= next_) poll();
        }
        hint_bar({{"A", tr("Select")}, {"B", tr("Back")}});
    }
private:
    void request() {
        scope_.reset(); code_ = {}; qr_ = {}; busy_ = true; expired_ = false; message_ = tr("Getting a code...");
        auto c = cancel_;
        scope_.run<ba::Login>([c] { return ba::generate(c.get()); }, [this](ba::Login l) {
            busy_ = false; code_ = std::move(l); message_ = code_.error.empty() ? tr("Waiting for scan") : code_.error;
            qr_ = qr::encode(code_.url); next_ = ui::time() + 3; expires_ = ui::time() + 180;
            if (qr_.size == 0 && code_.error.empty()) { message_ = tr("Could not render login code. Request a new code."); expired_ = true; }
        });
    }
    void poll() {
        if (ui::time() >= expires_) { expired_ = true; message_ = tr("Code expired. Request a new code."); return; }
        busy_ = true; auto c = cancel_; auto key = code_.key;
        scope_.run<ba::Login>([key, c] { return ba::poll(key, c.get()); }, [this](ba::Login l) {
            busy_ = false; next_ = ui::time() + 3;
            if (!l.error.empty()) { message_ = l.error; expired_ = true; }
            else if (l.state == 0) {
                if (store::active_profile() == profile_ && !store::kids_profile()) ba::save(l.session);
                reset_focus();
            } else if (l.state == 86038) { expired_ = true; message_ = tr("Code expired. Request a new code."); }
            else if (l.state == 86090) message_ = tr("Scanned. Confirm sign-in on your phone.");
            else if (l.state == 86101) message_ = tr("Waiting for scan");
            else { expired_ = true; message_ = tr("Unknown login status. Request a new code."); }
        });
    }
    std::string profile_, message_;
    ba::Login code_; qr::Code qr_; bool busy_ = false, expired_ = false;
    double next_ = 0, expires_ = 0;
    std::shared_ptr<std::atomic<bool>> cancel_ = std::make_shared<std::atomic<bool>>(false);
    tasks::Scope scope_;
};

class LibraryScreen : public app::Screen {
public:
    explicit LibraryScreen(bool history) : history_(history) { local(); }
    void on_enter() override { if (!remote_) local(); }
    ~LibraryScreen() override { cancel_->store(true); }
    app::Section section() const override { return app::SEC_BILIBILI; }
    void frame() override {
        const auto& t = theme(); const float x = content_x(), w = W - x - 40; const auto g = id("bili_library");
        page_.begin(id(g, "page")); float y = page_.y(52);
        text::draw(font::display, x, y, history_ ? tr("Bilibili history") : tr("Favorites"), t.text); y += 74;
        if (store::kids_profile()) { page_.end(200); return; }
        float bx = x;
        auto action = [&](const char* key, const char* label, int icon, bool selected=false) {
            float bw = measure_button(label, icon);
            const auto item = id(g, key);
            bool hit = button(item, Rect(bx, y, bw, 48), label, icon, selected?BTN_TAB:BTN_NORMAL, g);
            if (focused() == item) page_.focus_range(0, y + page_.scroll() + 60);
            bx += bw + 12; return hit;
        };
        if (action("local", tr("Local records"), ic::HISTORY,!remote_)) local();
        if (action("remote", tr("Bilibili account"), ic::PERSON) && !loading_) {
            if (!ba::session().ok()) app::push(make_bilibili_account());
            else { remote_ = true; folder_.clear(); title_.clear(); cursor_.clear(); items_.clear(); load(1); }
        }
        if (action("refresh", tr("Refresh"), ic::REFRESH) && !loading_) { if (remote_) { items_.clear(); cursor_.clear(); load(1); } else local(); }
        y += 68;
        text::draw(font::title, x, y, remote_ ? (title_.empty() ? tr("Bilibili account") : title_.c_str()) : tr("Local records"), t.text); y += 50;
        if (!error_.empty()) { y += text::draw_wrapped(font::body, Rect(x, y, w, 140), error_, t.bad, 4) + 20; }
        for (const auto& f : folders_) {
            const auto item = id(g, f.id.c_str());
            if (value_row(item, Rect(x, y, w, 60), f.title.c_str(), nullptr, ic::FOLDER, g)) {
                folder_ = f.id; title_ = f.title; folders_.clear(); load(1); break;
            }
            if (focused() == item) page_.focus_range(y + page_.scroll() - 16, y + page_.scroll() + 76);
            y += 72;
        }
        GridSpec spec; spec.count = (int)items_.size(); spec.cols = 4; spec.item_w = 268; spec.gap_x = 16; spec.loading = loading_;
        spec.item = [this](int i) { return bili::video_card(items_[i]); };
        spec.on_click = [this](int i) { bili::open(items_[i]); };
        if (!remote_) spec.on_x = [this](int i) {
            const auto v = items_[i];
            const std::weak_ptr<std::atomic<bool>> alive = scope_.token();
            show_menu(v.title, "", {{tr("Remove"), ic::REMOVE, [this, v, alive] {
                auto token = alive.lock(); if (!token || !token->load()) return;
                store::fav_set(history_ ? "bilibili_history" : "bilibili", {v.bvid, v.title, v.author, v.picture, ""}, false);
                local();
            }}});
        };
        y += grid(id(g, "videos"), x, y, spec, &page_);
        if (!loading_ && items_.empty() && folders_.empty() && error_.empty()) { empty_state(Rect(x, y, w, 160), ic::INFO, tr("Nothing here yet"), ""); y += 180; }
        if (!loading_ && more_ && items_.size() < 200) {
            if (button(id(g, "more"), Rect(x, y, w, 54), tr("Load more"), ic::REFRESH, BTN_NORMAL, g)) load(next_page_);
            if (focused() == id(g, "more")) page_.focus_range(y + page_.scroll(), y + page_.scroll() + 74);
            y += 74;
        }
        page_.end(y + page_.scroll() + 50);
        if (remote_) hint_bar({{"A", tr("Open")}, {"B", tr("Back")}});
        else hint_bar({{"A", tr("Open")}, {"X", tr("Remove")}, {"B", tr("Back")}});
    }
private:
    void local() {
        scope_.reset(); loading_ = false; remote_ = false; more_ = false; folders_.clear(); error_.clear();
        items_ = ba::local_library(history_); reset_focus();
    }
    void load(int page) {
        if (loading_) return;
        loading_ = true; error_.clear(); more_ = false;
        auto s = ba::session(); auto folder = folder_, cursor = cursor_; auto c = cancel_; bool hist = history_;
        scope_.run<ba::Library>([s, folder, cursor, c, hist, page] {
            if (hist) return ba::history(s, cursor, c.get());
            return folder.empty() ? ba::folders(s, 0, c.get()) : ba::favorites(s, folder, page, c.get());
        }, [this, s](ba::Library l) {
            loading_ = false;
            if (ba::session().cookie != s.cookie) { local(); return; }
            error_ = l.error; if (!l.ok) return;
            folders_ = std::move(l.folders);
            for (auto& v : l.videos) {
                if (items_.size() >= 200) break;
                if (std::none_of(items_.begin(), items_.end(), [&](const auto& old) { return old.bvid == v.bvid; })) items_.push_back(std::move(v));
            }
            cursor_ = l.cursor; next_page_ = l.next_page; more_ = history_ ? !cursor_.empty() : next_page_ > 0;
        });
    }
    bool history_, remote_ = false, loading_ = false, more_ = false; int next_page_ = 0;
    std::string error_, folder_, title_, cursor_;
    std::vector<bilibili::Video> items_; std::vector<ba::Folder> folders_;
    Page page_; tasks::Scope scope_;
    std::shared_ptr<std::atomic<bool>> cancel_ = std::make_shared<std::atomic<bool>>(false);
};
}
std::unique_ptr<app::Screen> make_bilibili_account() { return std::make_unique<AccountScreen>(); }
std::unique_ptr<app::Screen> make_bilibili_library(bool history) { return std::make_unique<LibraryScreen>(history); }
}

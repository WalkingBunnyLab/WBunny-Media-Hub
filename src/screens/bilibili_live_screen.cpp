#include "screens/bilibili_live_screen.hpp"
#include "screens/screens.hpp"
#include "services/bilibili_live.hpp"
#include "services/bilibili_account.hpp"
#include "core/i18n.hpp"
#include "core/store.hpp"
#include "core/tasks.hpp"
#include "core/util.hpp"
#include "player/player.hpp"
#include <algorithm>
#include <set>

namespace screens::bili {
namespace bl=bilibili::live;
using namespace ui;
namespace {
constexpr const char* FAVORITES="bilibili_live";
store::Fav favorite(const bl::Room& r){return {std::to_string(r.id),r.title,r.author,r.picture,r.area};}
bool saved(const bl::Room& r){return store::fav_has(FAVORITES,std::to_string(r.id));}
void toggle(const bl::Room& r){bool on=store::fav_toggle(FAVORITES,favorite(r));store::fav_trim(FAVORITES,200);toast(on?tr("Live room saved"):tr("Live room removed"),ic::FAVORITE);}
int quality(){int q=(int)store::get_int("bilibili_live_qn",150);return q==80||q==150||q==250||q==400?q:150;}
const char* quality_name(int q){return q==80?tr("Smooth"):q==250?tr("Ultra clear"):q==400?tr("Original"):tr("High definition");}
}
struct LiveView::Impl {
    enum Mode { HALL, BOOKMARKS, FOLLOWED } mode=HALL;
    bool active=false,loading=false,area_loading=false,dirty=false;
    int next=0,parent=0,area=0;
    std::string area_name,error,profile;
    bilibili::account::Session account;
    std::vector<bl::Room> rooms;
    std::vector<bl::Area> areas;
    tasks::Scope scope,area_scope;
    std::shared_ptr<std::atomic<bool>> cancel=std::make_shared<std::atomic<bool>>(false);
    ~Impl(){cancel->store(true);}
    void reset(){cancel->store(true);cancel=std::make_shared<std::atomic<bool>>(false);scope.reset();area_scope.reset();loading=false;area_loading=false;}
    void enter(){active=true;account=bilibili::account::session();profile=store::active_profile();mode=HALL;parent=area=0;area_name.clear();reset();load(1);}
    void sync(){if(!active)return;const auto now=bilibili::account::session();if(now.cookie!=account.cookie||now.uid!=account.uid||profile!=store::active_profile()){account=now;profile=store::active_profile();if(mode==FOLLOWED&&!account.ok())mode=HALL;reset();load(1);}}
    void select(Mode value){mode=value;reset();load(1);reset_focus();}
    void load(int page){
        if(!active||loading||store::kids_profile())return;
        if(page==1){rooms.clear();next=0;}
        error.clear();
        if(mode==BOOKMARKS){for(const auto& f:store::favs(FAVORITES)){auto n=bl::room_id(f.id);if(n>0){bl::Room r;r.id=n;r.title=f.title;r.author=f.subtitle;r.picture=f.image;r.area=f.extra;rooms.push_back(std::move(r));}}return;}
        if(mode==FOLLOWED&&!account.ok()){error=tr("Sign in to see followed live rooms.");return;}
        loading=true;auto c=cancel;auto cookie=account.cookie;auto requested_mode=mode;int p=parent,a=area;
        scope.run<bl::Rooms>([c,cookie,requested_mode,page,p,a]{return requested_mode==FOLLOWED?bl::followed(page,cookie,c.get()):bl::browse(page,p,a,cookie,c.get());},
            [this,page,cookie](bl::Rooms out){if(cookie!=bilibili::account::session().cookie){sync();return;}loading=false;
                if(out.auth_expired&&!cookie.empty()){bilibili::account::logout();sync();toast(tr("Bilibili login expired. Sign in again."),ic::INFO);return;}
                error=tr(out.error.c_str());if(!out.ok)return;size_t before=rooms.size();for(auto& r:out.items){if(rooms.size()>=200)break;if(std::none_of(rooms.begin(),rooms.end(),[&](const auto& old){return old.id==r.id;}))rooms.push_back(std::move(r));}
                next=page==1||rooms.size()>before?out.next_page:0;});
    }
    void choose_area(){
        if(areas.empty()&&!area_loading){area_loading=true;auto c=cancel;area_scope.run<bl::Areas>([c]{return bl::areas(c.get());},[this](bl::Areas out){area_loading=false;if(!out.ok){toast(tr(out.error.c_str()),ic::INFO);return;}areas=std::move(out.items);area_menu();});}
        else if(!area_loading)area_menu();
    }
    void set_area(int p,int a,const std::string& name){parent=p;area=a;area_name=name;select(HALL);}
    void area_menu(size_t page=0){
        const std::weak_ptr<std::atomic<bool>> alive=area_scope.token();
        std::vector<MenuItem> items={{tr("All live areas"),ic::LIVE_TV,[this,alive]{auto t=alive.lock();if(t&&t->load())set_area(0,0,"");}}};
        std::set<int> seen;std::vector<bl::Area> parents;
        for(const auto& a:areas)if(seen.insert(a.parent).second)parents.push_back(a);
        for(size_t i=page*5;i<std::min(parents.size(),page*5+5);++i){const auto a=parents[i];items.push_back({a.parent_name,ic::FOLDER,[this,a,alive]{auto t=alive.lock();if(!t||!t->load())return;subarea_menu(a.parent,a.parent_name);}});}
        if(page>0)items.push_back({tr("Previous page"),ic::FOLDER,[this,page,alive]{auto t=alive.lock();if(t&&t->load())area_menu(page-1);}});
        if((page+1)*5<parents.size())items.push_back({tr("Next page"),ic::FOLDER,[this,page,alive]{auto t=alive.lock();if(t&&t->load())area_menu(page+1);}});
        show_menu(tr("Live areas"),"",std::move(items));
    }
    void subarea_menu(int parent_id,const std::string& name,size_t page=0){
        const std::weak_ptr<std::atomic<bool>> alive=area_scope.token();
        std::vector<MenuItem> items={{tr("All")+std::string(" / ")+name,ic::LIVE_TV,[this,parent_id,name,alive]{auto t=alive.lock();if(t&&t->load())set_area(parent_id,0,name);}}};
        std::vector<bl::Area> children;for(const auto& a:areas)if(a.parent==parent_id&&a.id>0)children.push_back(a);
        for(size_t i=page*5;i<std::min(children.size(),page*5+5);++i){const auto a=children[i];items.push_back({a.name,ic::LIVE_TV,[this,a,alive]{auto t=alive.lock();if(t&&t->load())set_area(a.parent,a.id,a.parent_name+" / "+a.name);}});}
        if(page>0)items.push_back({tr("Previous page"),ic::FOLDER,[this,parent_id,name,page,alive]{auto t=alive.lock();if(t&&t->load())subarea_menu(parent_id,name,page-1);}});
        if((page+1)*5<children.size())items.push_back({tr("Next page"),ic::FOLDER,[this,parent_id,name,page,alive]{auto t=alive.lock();if(t&&t->load())subarea_menu(parent_id,name,page+1);}});
        show_menu(name,"",std::move(items));
    }
    void play(const bl::Room& room){if(!store::kids_profile())play_video(bl::make_source(room,quality(),bilibili::account::session().cookie));}
    void prompt(){
        const std::weak_ptr<std::atomic<bool>> alive=scope.token();
        prompt_text(tr("Open live room / URL"),"","https://live.bilibili.com/…",[this,alive](std::string input){auto t=alive.lock();if(!t||!t->load())return;auto id=bl::room_id(input);if(!id){toast(tr("Enter a live room number or live.bilibili.com URL."),ic::INFO);return;}bl::Room r;r.id=id;r.title=tr("Live room")+std::string(" ")+std::to_string(id);
            show_menu(r.title,"",{{tr("Play"),ic::PLAY,[this,alive,r]{auto a=alive.lock();if(a&&a->load())play(r);}},{tr("Save room and play"),ic::FAVORITE,[this,alive,r]{auto a=alive.lock();if(!a||!a->load())return;store::fav_set(FAVORITES,favorite(r),true);store::fav_trim(FAVORITES,200);play(r);}}});});
    }
};
LiveView::LiveView():impl_(std::make_unique<Impl>()){}
LiveView::~LiveView()=default;
void LiveView::activate(){impl_->enter();}
void LiveView::refresh(){impl_->reset();impl_->load(1);}
void LiveView::deactivate(){impl_->active=false;impl_->reset();}
void LiveView::on_enter(){impl_->sync();if(impl_->active&&impl_->mode==Impl::BOOKMARKS)impl_->load(1);}
void LiveView::open_room(){impl_->prompt();}
float LiveView::frame(float x,float y,float w,Page& page,Id group){
    auto& v=*impl_;v.sync();if(v.dirty){v.dirty=false;v.load(1);}float start=y,cx=x;Id top=id(group,"live_actions");
    auto action=[&](const char* key,const char* label,int icon,bool selected=false){float width=measure_button(label,icon)+14;if(cx+width>x+w&&cx>x){cx=x;y+=58;}bool hit=button(id(top,key),Rect(cx,y,width,44),label,icon,selected?BTN_TAB:BTN_NORMAL,top);cx+=width+12;return hit;};
    if(action("hall",tr("Live lobby"),ic::LIVE_TV,v.mode==Impl::HALL))v.select(Impl::HALL);
    if(action("areas",v.area_loading?tr("Loading..."):v.area_name.empty()?tr("Live areas"):v.area_name.c_str(),ic::FOLDER))v.choose_area();
    if(action("saved",tr("Saved rooms"),ic::FAVORITE,v.mode==Impl::BOOKMARKS))v.select(Impl::BOOKMARKS);
    if(v.account.ok()&&action("followed",tr("Followed live"),ic::PERSON,v.mode==Impl::FOLLOWED))v.select(Impl::FOLLOWED);
    if(action("quality",quality_name(quality()),ic::HD)){int q=quality();store::set_int("bilibili_live_qn",q==80?150:q==150?250:q==250?400:80);}
    if(action("account",tr("Account"),ic::PERSON))app::push(make_bilibili_account());
    y+=70;if(focus_in_group(top))page.focus_range(0,y+page.scroll());
    text::draw(font::title,x,y,v.mode==Impl::BOOKMARKS?tr("Saved live rooms"):v.mode==Impl::FOLLOWED?tr("Followed live rooms"):tr("Bilibili live lobby"),theme().text);y+=52;
    if(v.rooms.empty()&&!v.loading){
        if(v.error.empty())empty_state(Rect(x,y,w,200),ic::LIVE_TV,tr("No live rooms here"),tr("Choose another area or refresh."));
        else if(empty_state_action(id(group,"live_retry"),Rect(x,y,w,230),ic::WIFI_OFF,tr("Couldn't load live rooms"),v.error.c_str()))v.load(1);
        y+=240;
    }else{
        GridSpec spec;spec.count=(int)v.rooms.size();spec.cols=4;spec.item_w=268;spec.gap_x=16;spec.loading=v.loading;
        spec.item=[&v](int i){const auto& r=v.rooms[i];CardInfo c;c.title=r.title.empty()?tr("Live room")+std::string(" ")+std::to_string(r.id):r.title;c.subtitle=r.author;if(!r.area.empty())c.subtitle+=(c.subtitle.empty()?"":" / ")+r.area;c.image=r.picture;c.icon=ic::LIVE_TV;c.live=v.mode!=Impl::BOOKMARKS;c.badge=r.viewers>0?util::format_count(r.viewers):"";c.favorite=saved(r);return c;};
        spec.on_click=[&v](int i){v.play(v.rooms[i]);};
        spec.on_focus=[&v](int i){set_backdrop(v.rooms[i].picture);};
        spec.on_x=[&v](int i){auto r=v.rooms[i];toggle(r);if(v.mode==Impl::BOOKMARKS)v.dirty=true;};
        y+=grid(id(group,"live_rooms"),x,y,spec,&page);
        if(!v.loading&&v.next&&v.rooms.size()<200){auto iid=id(group,"live_more");if(button(iid,Rect(x,y+12,w,54),v.error.empty()?tr("Load more"):tr("Try again"),ic::REFRESH,BTN_NORMAL,group))v.load(v.next);if(focused()==iid)page.focus_range(y+page.scroll(),y+page.scroll()+90);y+=88;}
        if(!v.error.empty())y+=text::draw_wrapped(font::body,Rect(x,y,w,120),v.error,theme().text2,3)+20;
    }
    return y-start;
}
}

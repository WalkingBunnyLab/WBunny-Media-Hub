#include "services/bilibili_live.hpp"
#include "core/http.hpp"
#include "player/player.hpp"
#include <cstdio>
int main(){
    namespace bl=bilibili::live;http::init("");
    auto feed=bl::browse(1,0,0,"",nullptr);if(!feed.ok||feed.items.empty()){fprintf(stderr,"Lobby: %s\n",feed.error.c_str());return 1;}
    printf("PASS public lobby: %zu rooms, next page %d\n",feed.items.size(),feed.next_page);
    auto areas=bl::areas(nullptr);if(!areas.ok||areas.items.empty()){fprintf(stderr,"Areas: %s\n",areas.error.c_str());return 1;}
    printf("PASS areas: %zu entries\n",areas.items.size());
    auto second=bl::browse(2,0,0,"",nullptr);if(!second.ok)return 1;printf("PASS second page: %zu rooms\n",second.items.size());
    auto filtered=bl::browse(1,areas.items.front().parent,0,"",nullptr);if(!filtered.ok)return 1;printf("PASS filtered area: %zu rooms\n",filtered.items.size());
    bool readable=false;for(size_t i=0;i<feed.items.size()&&i<4;++i){auto source=bl::make_source(feed.items[i],150,"");std::string error;if(source.resolve(source,error)){printf("PASS fresh H.264 live media readable, room %lld\n",(long long)feed.items[i].id);readable=true;break;}fprintf(stderr,"Room unavailable: %s\n",error.c_str());}
    http::shutdown();return readable?0:1;
}

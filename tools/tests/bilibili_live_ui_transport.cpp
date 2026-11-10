#include "core/http.hpp"
#include <cstdio>
#include <cstdlib>
extern "C" http::Response __wrap__ZN4http7performERKNS_7RequestE(const http::Request& r){
    http::Response out;out.status=200;
    std::string scenario=getenv("WBUNNY_LIVE_SCENARIO")?getenv("WBUNNY_LIVE_SCENARIO"):"";
    if(r.url.find("/qrcode/generate")!=std::string::npos){puts("QA account QR requested");out.body=R"({"code":0,"data":{"url":"https://account.bilibili.com/h5/account-h5/auth/scan-web?test=fixture","qrcode_key":"fixture"}})";return out;}
    if(r.url.find("/qrcode/poll")!=std::string::npos){out.body=R"({"code":0,"data":{"code":86101}})";return out;}
    if(r.url.find("/feedV2?")!=std::string::npos){
        if(scenario=="network"){out.status=503;puts("QA live network error");}
        else {puts(r.url.find("parent_area_id=2")!=std::string::npos?"QA live filtered":"QA live lobby");out.body=R"({"code":0,"data":{"has_more":false,"card_list":[{"card_type":"small_card_v1","card_data":{"small_card_v1":{"roomid":6,"title":"Fixture live room","uname":"Fixture UP","area_name":"Games","online":12345}}},{"card_type":"small_card_v1","card_data":{"small_card_v1":{"roomid":7,"title":"Second room","uname":"Second UP"}}}]}})";}
    }else if(r.url.find("/getAreaList?")!=std::string::npos){puts("QA live areas");out.body=R"({"code":0,"data":{"list":[{"id":2,"name":"Games","area_list":[{"id":10,"name":"Console"}]},{"id":3,"name":"Music"},{"id":4,"name":"Sports"},{"id":5,"name":"Talk"},{"id":6,"name":"Radio"},{"id":7,"name":"Other"}]}})";
    }else if(r.url.find("/xfetter/GetWebList?")!=std::string::npos){
        if(scenario=="expired"){puts("QA live expiry");out.body=R"({"code":-101})";}
        else{puts("QA followed live");out.body=R"({"code":0,"data":{"live_count":1,"list":[{"roomid":8,"live_status":1,"title":"Followed fixture room","uname":"Followed UP"}]}})";}
    }else if(r.url.find("/getRoomPlayInfo?")!=std::string::npos){puts("QA offline room");out.body=R"({"code":0,"data":{"room_id":6,"live_status":0}})";
    }else if(r.url.find("/x/web-interface/popular?")!=std::string::npos){puts("QA popular");out.body=R"({"code":0,"data":{"no_more":true,"list":[]}})";
    }else out.body=R"({"code":0,"data":{}})";
    return out;
}

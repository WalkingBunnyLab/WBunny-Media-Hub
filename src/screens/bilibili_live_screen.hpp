#pragma once
#include <memory>
#include "screens/widgets.hpp"
namespace screens::bili {
// Embedded below the common Bilibili tabs; switching tabs cancels its work.
class LiveView {
public:
    LiveView();
    ~LiveView();
    void activate();
    void refresh();
    void deactivate();
    void on_enter();
    void open_room();
    float frame(float x,float y,float w,Page& page,ui::Id group);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}

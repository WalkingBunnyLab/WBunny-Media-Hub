#pragma once

#include "screens/widgets.hpp"
#include "services/bilibili.hpp"

namespace screens::bili {
CardInfo video_card(const bilibili::Video& video);
void open(const bilibili::Video& video);
}  // namespace screens::bili

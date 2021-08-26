#include "PixelEvent.h"
#include "../../util/constants.h"
#include "../../util/util.h"

PixelEvent::PixelEvent(int number, std::pair<int, int> position) {
  data.push_back(PIXEL_EVENT_LEN);
  data.push_back(PIXEL_EVENT);
  data.push_back(number);                        // only one byte
  util::serialize(data, htonl(position.second)); // y
  util::serialize(data, htonl(position.first));  // x
}

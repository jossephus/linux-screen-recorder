#ifndef SCREEN_RECORDER_DIMENSIONS_H
#define SCREEN_RECORDER_DIMENSIONS_H

#include <algorithm>
#include <utility>

namespace screen_recorder {
namespace utils {

inline std::pair<int, int> MakeEvenDimensions(int width, int height) {
  width = std::max(2, width);
  height = std::max(2, height);
  if ((width & 1) != 0) {
    --width;
  }
  if ((height & 1) != 0) {
    --height;
  }
  return {width, height};
}

}  // namespace utils
}  // namespace screen_recorder

#endif  // SCREEN_RECORDER_DIMENSIONS_H

// cozmoConfig.cpp

#include "anki/cozmo/shared/cozmoConfig.h"
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>

namespace Anki {
namespace Cozmo {

// define the globals declared in cozmoConfig.h
s32 gFaceDisplayWidth             = 184;
s32 gFaceDisplayHeight            = 96;
s32 gFaceDisplayNumPixels         = 184 * 96;

// helper: read the hardware version from the EMR partition
static uint32_t GetHardwareVersion() {
  int fd = open("/dev/mmcblk0p29", O_RDONLY);
  if(fd < 0) {
    // failed to open hw partition; default to 0 (could also choose a safe default)
    return 0;
  }
  uint32_t emr_data[8] = {0};
  int res = read(fd, emr_data, sizeof(emr_data));
  close(fd);
  if(res < 0) {
    return 0;
  }
  return emr_data[1];  // assume the hardware version is stored here
}

// init function to adjust display dimensions based on hardware
void InitCozmoConfig() {
  uint32_t hwVer = GetHardwareVersion();
  
  if(hwVer > 7) {
    // for hw versions above 7, use the nv3022/3022 screen (e.g. 160x80)
    gFaceDisplayWidth            = 160;
    gFaceDisplayHeight           = 80;
  } else {
    // for hw versions 7 and below, use the st7789 screen (e.g. 184x96)
    gFaceDisplayWidth            = 184;
    gFaceDisplayHeight           = 96;
  }
  
  gFaceDisplayNumPixels = gFaceDisplayWidth * gFaceDisplayHeight;
}

} // namespace Cozmo
} // namespace Anki


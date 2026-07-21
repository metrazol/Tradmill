#pragma once
#include "board_select.h"

#if defined(BOARD_GC9A01)
  #include "config_gc9a01.h"
#elif defined(BOARD_ZX2D80CE02S)
  #include "config_zx2d80ce02s.h"
#elif defined(BOARD_DIS08070H)
  #include "config_dis08070h.h"
#elif defined(BOARD_ESP32_2432S028R)
  #include "config_esp32_2432s028r.h"
#else
  #error "No board selected — open board_select.h and uncomment your board."
#endif

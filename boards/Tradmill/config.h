#pragma once
#include "board_select.h"

#if defined(BOARD_GC9A01)
  #include "config_gc9a01.h"
#elif defined(BOARD_ZX2D80CE02S)
  #include "config_zx2d80ce02s.h"
#elif defined(BOARD_DIS08070H)
  #include "config_dis08070h.h"
#else
  #error "No board selected — open board_select.h and uncomment your board."
#endif

#include "vt_cmap.h"

uint8_t VtDefaultCLUT[32][3] = {
  {0, 0, 0}
  ,
  {15, 0, 0}
  ,
  {0, 15, 0}
  ,
  {15, 15, 0}
  ,
  {0, 0, 15}
  ,
  {15, 0, 15}
  ,
  {0, 15, 15}
  ,
  {15, 15, 15}
  ,

  {0, 0, 0}
  ,                             //should be transparent. values meaningless
  {7, 0, 0}
  ,
  {0, 7, 0}
  ,
  {7, 7, 0}
  ,
  {0, 0, 7}
  ,
  {7, 0, 7}
  ,
  {0, 7, 7}
  ,
  {7, 7, 7}
  ,

  {15, 0, 5}
  ,
  {15, 7, 0}
  ,
  {0, 15, 7}
  ,
  {15, 15, 11}
  ,
  {0, 12, 10}
  ,
  {5, 0, 0}
  ,
  {6, 5, 2}
  ,
  {12, 7, 7}
  ,

  {3, 3, 3}
  ,
  {15, 7, 7}
  ,
  {7, 15, 7}
  ,
  {15, 15, 7}
  ,
  {7, 7, 15}
  ,
  {15, 7, 15}
  ,
  {7, 15, 15}
  ,
  {13, 13, 13}
};

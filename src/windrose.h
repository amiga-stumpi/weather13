#ifndef WEATHER13_WINDROSE_H
#define WEATHER13_WINDROSE_H

#include <exec/types.h>
#include <graphics/rastport.h>

const char *W13_WindDirName(int wind_dir_deg);
void W13_DrawWindRose(struct RastPort *rp, int cx, int cy, int radius,
                      int wind_dir_deg, int wind_speed, WORD jitter_x, WORD jitter_y, UBYTE pen);

#endif

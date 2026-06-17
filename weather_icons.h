#ifndef WEATHER13_ICONS_H
#define WEATHER13_ICONS_H

/*
 * Weather13 generated icon assets.
 *
 * Target: AmigaOS 1.3 / classic C.
 *
 * Format:
 * - width/height in pixels
 * - depth = 2, 3 or 4 bitplanes
 * - row_words = ((width + 15) / 16)
 * - image data is plane-major:
 *      plane 0 rows, then plane 1 rows, etc.
 * - each row consists of row_words UWORDs
 * - leftmost pixel of each 16-pixel word is bit 15
 * - mask is 1 bitplane, row-major, same row_words
 * - pen 0 is transparent/background
 *
 * Suggested rendering:
 * - allocate temporary BitMap with matching depth
 * - copy the planar data into planes
 * - blit through mask into Window->RPort
 * - or draw icons manually from the packed data if desired
 *
 * Weather code mapping:
 *   W13_WEATHER_SUN      clear
 *   W13_WEATHER_PARTLY   partly cloudy
 *   W13_WEATHER_CLOUD    cloudy/overcast
 *   W13_WEATHER_RAIN     rain/showers
 *   W13_WEATHER_STORM    thunderstorm
 *   W13_WEATHER_FOG      fog/mist
 *   W13_WEATHER_SNOW     snow
 *   W13_WEATHER_UNKNOWN  fallback
 */

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

#define W13_WEATHER_SUN      0
#define W13_WEATHER_PARTLY   1
#define W13_WEATHER_CLOUD    2
#define W13_WEATHER_RAIN     3
#define W13_WEATHER_STORM    4
#define W13_WEATHER_FOG      5
#define W13_WEATHER_SNOW     6
#define W13_WEATHER_UNKNOWN  7
#define W13_WEATHER_COUNT    8

typedef struct Weather13Icon {
    UWORD width;
    UWORD height;
    UWORD depth;
    UWORD row_words;
    const UWORD *planes;
    const UWORD *mask;
} Weather13Icon;

const Weather13Icon *Weather13_GetIcon(UWORD weather_code, UWORD wanted_size, UWORD screen_depth);

#endif /* WEATHER13_ICONS_H */

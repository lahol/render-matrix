#include "util-colors.h"

double util_colors_basic_table[UTIL_COLORS_COUNT + 1][3] = {
    /* { 1.0, 1.0, 1.0 }, */
/*        { 1.0, 0.0, 1.0 },*/
    { 0.0, 0.0, 0.0 },
    { 0.0, 0.0, 1.0 },
    { 0.0, 1.0, 1.0 },
    { 0.0, 1.0, 0.0 },
    { 1.0, 1.0, 0.0 },
    { 1.0, 0.0, 0.0 },
    { 0.0, 0.0, 0.0 }, /* currently only for index, although this should be caught in the next if clause */
};


/* get rgb values for (101->001->011->010->110->100)
 * [magenta -> blue -> cyan -> green -> yellow -> red]
 * @in: hue in [0,1]
 * @out: rgb [0,1]^3
 */
void util_colors_gradient_rgb(double hue, double *rgb)
{
    if (hue >= 1.0) {
        rgb[0] = util_colors_basic_table[UTIL_COLORS_COUNT][0];
        rgb[1] = util_colors_basic_table[UTIL_COLORS_COUNT][1];
        rgb[2] = util_colors_basic_table[UTIL_COLORS_COUNT][2];
        return;
    }
    if (hue <= 0.0) {
        rgb[0] = util_colors_basic_table[0][0];
        rgb[1] = util_colors_basic_table[0][1];
        rgb[2] = util_colors_basic_table[0][2];
        return;
    }

    int index = (int)((UTIL_COLORS_COUNT) * hue);       /* floor */
    double lambda = ((UTIL_COLORS_COUNT) * hue - index); /* frac */

    rgb[0] = (1.0 - lambda) * util_colors_basic_table[index][0] + lambda * util_colors_basic_table[index + 1][0];
    rgb[1] = (1.0 - lambda) * util_colors_basic_table[index][1] + lambda * util_colors_basic_table[index + 1][1];
    rgb[2] = (1.0 - lambda) * util_colors_basic_table[index][2] + lambda * util_colors_basic_table[index + 1][2];
}



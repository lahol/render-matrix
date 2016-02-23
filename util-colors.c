#include "util-colors.h"

double util_colors_basic_table_color[] = {
    /* { 1.0, 1.0, 1.0 }, */
/*        { 1.0, 0.0, 1.0 },*/
    0.0, 0.0, 0.0,
    0.0, 0.0, 1.0,
    0.0, 1.0, 1.0,
    0.0, 1.0, 0.0,
    1.0, 1.0, 0.0,
    1.0, 0.0, 0.0,
    0.0, 0.0, 0.0, /* currently only for index, although this should be caught in the next if clause */
};

double util_colors_basic_table_grayscale[] = {
    1.0, 1.0, 1.0,
    0.8, 0.8, 0.8,
    0.6, 0.6, 0.6,
    0.4, 0.4, 0.4,
    0.2, 0.2, 0.2,
    0.0, 0.0, 0.0,
    1.0, 1.0, 1.0,
};

double *util_colors_basic_table = util_colors_basic_table_color;

#define UTIL_COLORS_COUNT (6)
unsigned int color_count = UTIL_COLORS_COUNT;

double *util_colors_get_basic_table(unsigned int *count)
{
    if (count)
        *count = color_count;
    return util_colors_basic_table;
}

void util_colors_set_grayscale(int use_grayscale)
{
    if (use_grayscale) {
        util_colors_basic_table = util_colors_basic_table_grayscale;
        color_count = 6;
    }
    else {
        util_colors_basic_table = util_colors_basic_table_color;
        color_count = UTIL_COLORS_COUNT;
    }
}
/* get rgb values for (101->001->011->010->110->100)
 * [magenta -> blue -> cyan -> green -> yellow -> red]
 * @in: hue in [0,1]
 * @out: rgb [0,1]^3
 */
void util_colors_gradient_rgb(double hue, double *rgb)
{
    if (hue >= 1.0) {
        rgb[0] = util_colors_basic_table[UTIL_COLORS_COUNT * 3];
        rgb[1] = util_colors_basic_table[UTIL_COLORS_COUNT * 3 + 1];
        rgb[2] = util_colors_basic_table[UTIL_COLORS_COUNT * 3 + 2];
        return;
    }
    if (hue <= 0.0) {
        rgb[0] = util_colors_basic_table[0];
        rgb[1] = util_colors_basic_table[1];
        rgb[2] = util_colors_basic_table[2];
        return;
    }

    int index = (int)((UTIL_COLORS_COUNT) * hue);       /* floor */
    double lambda = ((UTIL_COLORS_COUNT) * hue - index); /* frac */

    rgb[0] = (1.0 - lambda) * util_colors_basic_table[index * 3 + 0] + lambda * util_colors_basic_table[index * 3 + 3];
    rgb[1] = (1.0 - lambda) * util_colors_basic_table[index * 3 + 1] + lambda * util_colors_basic_table[index * 3 + 4];
    rgb[2] = (1.0 - lambda) * util_colors_basic_table[index * 3 + 2] + lambda * util_colors_basic_table[index * 3 + 5];
}



#pragma once

#define UTIL_COLORS_COUNT (6)

extern double util_colors_basic_table[UTIL_COLORS_COUNT + 1][3];

void util_colors_gradient_rgb(double hue, double *rgb);

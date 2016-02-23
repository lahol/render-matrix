#pragma once

/*#define UTIL_COLORS_COUNT (6)*/

/*extern double util_colors_basic_table[UTIL_COLORS_COUNT + 1][3];*/

double *util_colors_get_basic_table(unsigned int *count);

void util_colors_gradient_rgb(double hue, double *rgb);

void util_colors_set_grayscale(int use_grayscale);

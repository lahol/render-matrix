#pragma once

typedef struct {
    double x;
    double y;
    double width;
    double height;
} UtilRectangle;

void util_rectangle_bounds(UtilRectangle *dst, UtilRectangle *src1, UtilRectangle *src2);
void util_rectangle_crop(UtilRectangle *rect, UtilRectangle *crop);

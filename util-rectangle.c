#include <stdlib.h>

#include "util-rectangle.h"

void util_rectangle_bounds(UtilRectangle *dst, UtilRectangle *src1, UtilRectangle *src2)
{
    if ((src1 == NULL && src2 == NULL) || dst == NULL)
        return;
    if (src1 == NULL) {
        *dst = *src2;
        return;
    }
    if (src2 == NULL) {
        *dst = *src1;
        return;
    }

    UtilRectangle tmp;

    tmp.x = src1->x < src2->x ? src1->x : src2->x;
    tmp.y = src1->y < src2->y ? src1->y : src2->y;
    tmp.width = (src1->x + src1->width > src2->x + src2->width) ?
        src1->x + src1->width - tmp.x : src2->x + src2->width - tmp.x;
    tmp.height = (src1->y + src1->height > src2->y + src2->height) ?
        src1->y + src1->height - tmp.y : src2->y + src2->height - tmp.y;

    *dst = tmp;
}

void util_rectangle_crop(UtilRectangle *rect, UtilRectangle *crop)
{
    if (rect == NULL || crop == NULL)
        return;
    double xr[2] = { rect->x, rect->x + rect->width - 1.0f };
    double yr[2] = { rect->y, rect->y + rect->height - 1.0f };

    if (xr[0] < crop->x)
        xr[0] = crop->x;
    if (xr[1] >= crop->x + crop->width)
        xr[1] = crop->x + crop->width - 1.0f;
    if (yr[0] < crop->y)
        yr[0] = crop->y;
    if (yr[1] >= crop->y + crop->height)
        yr[1] = crop->y + crop->height - 1.0f;

    rect->x = xr[0];
    rect->y = yr[0];
    rect->width = xr[0] < xr[1] ? xr[1]-xr[0]+1.0f : 0.0f;
    rect->height = yr[0] < yr[1] ? yr[1]-yr[0]+1.0f : 0.0f;
}

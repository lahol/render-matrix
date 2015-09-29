#pragma once

#include <X11/X.h>
#include "matrix.h"
#include "util-rectangle.h"

typedef struct _GraphicsHandle GraphicsHandle;

/* x, y, align, string, userdata */
typedef enum {
    TiksAlignLeft = 1 << 0,
    TiksAlignRight = 1 << 1,
    TiksAlignTop = 1 << 2,
    TiksAlignBottom = 1 << 3
} TiksAlign;
typedef void (*GraphicsTiksCallback)(double, double, guint8, const gchar *, gpointer);

typedef enum {
    ArcBallRestrictionNone = 0,
    ArcBallRestrictionVertical,
    ArcBallRestrictionHorizontal
} ArcBallRestriction;

GraphicsHandle *graphics_init(void);
void graphics_cleanup(GraphicsHandle *handle);
void graphics_render(GraphicsHandle *handle, GraphicsTiksCallback callback, gpointer userdata);

void graphics_set_window(GraphicsHandle *handle, Window window);
void graphics_set_window_size(GraphicsHandle *handle, int width, int height);

void graphics_set_camera(GraphicsHandle *handle, double azimuth, double elevation, double tilt);
void graphics_camera_zoom(GraphicsHandle *handle, gint steps);

void graphics_camera_move_start(GraphicsHandle *handle, double dx, double dy);
void graphics_camera_move_update(GraphicsHandle *handle, double dx, double dy);
void graphics_camera_move_finish(GraphicsHandle *handle, double dx, double dy);

void graphics_camera_arcball_rotate_start(GraphicsHandle *handle, double x, double y);
void graphics_camera_arcball_rotate_update(GraphicsHandle *handle, double x, double y, ArcBallRestriction rst);
void graphics_camera_arcball_rotate_finish(GraphicsHandle *handle, double x, double y, ArcBallRestriction rst);

void graphics_set_matrix_data(GraphicsHandle *handle, Matrix *matrix);
void graphics_update_matrix_data(GraphicsHandle *handle);

void graphics_save_buffer_to_file(GraphicsHandle *handle, const gchar *filename);
void graphics_get_render_area(GraphicsHandle *handle, UtilRectangle *render_area);

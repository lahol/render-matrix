#pragma once

#include <X11/X.h>
#include "matrix.h"

typedef struct _GraphicsHandle GraphicsHandle;

typedef enum {
    ArcBallRestrictionNone = 0,
    ArcBallRestrictionVertical,
    ArcBallRestrictionHorizontal
} ArcBallRestriction;

GraphicsHandle *graphics_init(void);
void graphics_cleanup(GraphicsHandle *handle);
void graphics_render(GraphicsHandle *handle);

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

void graphics_save_buffer_to_file(GraphicsHandle *handle, const gchar *filename);

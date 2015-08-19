#pragma once

#include <X11/X.h>
#include "matrix.h"

typedef struct _GraphicsHandle GraphicsHandle;

GraphicsHandle *graphics_init(void);
void graphics_cleanup(GraphicsHandle *handle);
void graphics_render(GraphicsHandle *handle);

void graphics_set_window(GraphicsHandle *handle, Window window);
void graphics_set_window_size(GraphicsHandle *handle, int width, int height);

void graphics_set_camera(GraphicsHandle *handle, double azimuth, double elevation);
void graphics_camera_zoom(GraphicsHandle *handle, gint steps);
void graphics_camera_move(GraphicsHandle *handle, double dx, double dy);

void graphics_set_matrix_data(GraphicsHandle *handle, Matrix *matrix);

void graphics_save_buffer_to_file(GraphicsHandle *handle, const gchar *filename);

#pragma once

#include <X11/X.h>

typedef struct _GraphicsHandle GraphicsHandle;

GraphicsHandle *graphics_init(void);
void graphics_cleanup(GraphicsHandle *handle);
void graphics_render(GraphicsHandle *handle);

void graphics_set_window(GraphicsHandle *handle, Window window);
void graphics_set_window_size(GraphicsHandle *handle, int width, int height);

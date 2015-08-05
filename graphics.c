#include <X11/X.h>
#include <X11/Xlib.h>

#include <GL/gl.h>
/*#include <GL/glext.h>*/
#include <GL/glx.h>
/*#include <GL/glxext.h>*/

#include <memory.h>
#include <stdio.h>

#include <glib.h>
#include <gdk/gdkx.h>

#include "graphics.h"

struct _GraphicsHandle {
    Display *display;
    Window window;
    int width;
    int height;
    GLXContext glx_context;
    GLXWindow glx_window;
};

/* get rgb values for (101->001->011->010->110->100)
 * [magenta -> blue -> cyan -> green -> yellow -> red]
 * @in: hue in [0,1]
 * @out: rgb [0,1]^3
 */
void color_gradient_rgb(double hue, double *rgb)
{
#define _N_COLORS (7)

    static double basic_table[_N_COLORS+1][3] = {
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

    if (hue >= 1.0) {
        rgb[0] = 1.0; rgb[1] = 0.0; rgb[2] = 0.0;
        return;
    }
    if (hue <= 0.0) {
        rgb[0] = basic_table[0][0]; rgb[1] = basic_table[0][1], rgb[2] = basic_table[0][2];
        return;
    }

    int index = (int)((_N_COLORS - 1) * hue);       /* floor */
    double lambda = ((_N_COLORS - 1) * hue - index); /* frac */

    rgb[0] = (1.0 - lambda) * basic_table[index][0] + lambda * basic_table[index + 1][0];
    rgb[1] = (1.0 - lambda) * basic_table[index][1] + lambda * basic_table[index + 1][1];
    rgb[2] = (1.0 - lambda) * basic_table[index][2] + lambda * basic_table[index + 1][2];

#undef _N_COLORS
}

GraphicsHandle *graphics_init(void)
{
    GraphicsHandle *handle = g_malloc0(sizeof(GraphicsHandle));
    XVisualInfo *vi = NULL;

    int attribs[] = {
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
        0
    };

    handle->display = gdk_x11_display_get_xdisplay(gdk_display_get_default());

    GLint majVer, minVer;

    if (!glXQueryExtension(handle->display, &majVer, &minVer)) {
        fprintf(stderr, "OpenGL not supported by default display.\n");
        goto err;
    }
    fprintf(stderr, "Supported GLX version: %d.%d\n", majVer, minVer);

    vi = glXChooseVisual(handle->display,
            gdk_x11_screen_get_screen_number(gdk_display_get_default_screen(gdk_display_get_default())),
            attribs);
    if (vi == NULL) {
        fprintf(stderr, "Could not configure OpenGL.\n");
        goto err;
    }

    handle->glx_context = glXCreateContext(handle->display, vi, NULL, True);

    XFree(vi);
    return handle;

err:
    if (vi)
        XFree(vi);
    g_free(handle);
    return NULL;
}

void graphics_cleanup(GraphicsHandle *handle)
{
    if (handle) {
        glXDestroyContext(handle->display, handle->glx_context);
        g_free(handle);
    }
}

void graphics_render(GraphicsHandle *handle)
{
    if (G_UNLIKELY(handle->window == 0))
        return;
    glXMakeCurrent(handle->display, handle->window, handle->glx_context);

    glClear(GL_COLOR_BUFFER_BIT);

    double rgb[3];
    double h;
    glBegin(GL_QUADS);

    for (h=0.0; h <= 1.0; h += 0.001) {
        color_gradient_rgb(h, rgb);
        glColor3f(rgb[0], rgb[1], rgb[2]);
        glVertex2f(h, 0);
        glVertex2f(h+0.001, 0);
        glVertex2f(h+0.001, 1);
        glVertex2f(h, 1);
    }

    glEnd();

    glXSwapBuffers(handle->display, handle->window);
}

void graphics_set_window(GraphicsHandle *handle, Window window)
{
    g_return_if_fail(handle != NULL);

    handle->window = window;
}

void graphics_set_window_size(GraphicsHandle *handle, int width, int height)
{
    handle->width = width;
    handle->height = height;
    if (G_UNLIKELY(handle->window == 0))
        return;
    if (!glXMakeCurrent(handle->display, handle->window, handle->glx_context))
        return;

    glViewport(0, 0, width, height);
}

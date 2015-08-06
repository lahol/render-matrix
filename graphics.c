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
#include "util-projection.h"

struct _GraphicsHandle {
    int width;
    int height;

    double projection_matrix[16];
    double rotation_matrix[16];
    double scale_vector[4];
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

void graphics_update_camera(GraphicsHandle *handle)
{
    if (!handle->width || !handle->height)
        return;
    double tmp[16];
    util_matrix_identify(tmp);
    util_matrix_multiply(handle->rotation_matrix, tmp, handle->projection_matrix);
    util_scale_matrix(handle->projection_matrix, handle->scale_vector);
}

void graphics_set_camera(GraphicsHandle *handle, double azimuth, double elevation)
{
    g_return_if_fail(handle != NULL);

    util_matrix_identify(handle->rotation_matrix);
    util_rotate_matrix(handle->rotation_matrix, azimuth, UTIL_AXIS_Z);
    util_rotate_matrix(handle->rotation_matrix, elevation, UTIL_AXIS_X);

    graphics_update_camera(handle);
}

void graphics_recalc_scale_vector(GraphicsHandle *handle)
{
    if (!handle->width || !handle->height)
        return;
    handle->scale_vector[0] = (2.0f/handle->width); /* *zoom_factor */
    handle->scale_vector[1] = (2.0f/handle->height); /* *zoom_factor */
    handle->scale_vector[2] = 0.001f; /* *zoom_factor */
    handle->scale_vector[3] = 1.0f;
}

GraphicsHandle *graphics_init(void)
{
    GraphicsHandle *handle = g_malloc0(sizeof(GraphicsHandle));

    util_matrix_identify(handle->projection_matrix);
    util_matrix_identify(handle->rotation_matrix);
    handle->scale_vector[0] = 1.0;
    handle->scale_vector[1] = 1.0;
    handle->scale_vector[2] = 1.0;
    handle->scale_vector[3] = 1.0;

    return handle;
}

void graphics_cleanup(GraphicsHandle *handle)
{
    g_free(handle);
}

void graphics_render(GraphicsHandle *handle)
{
    glClear(GL_COLOR_BUFFER_BIT);

    glViewport(0, 0, handle->width, handle->height);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixd(handle->projection_matrix);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    double rgb[3];
    double h;
    glBegin(GL_QUADS);

    for (h=0.0; h <= 100.0; h += 0.1) {
        color_gradient_rgb(h*0.01, rgb);
        glColor3f(rgb[0], rgb[1], rgb[2]);
        glVertex2f(h, 0);
        glVertex2f(h+0.1, 0);
        glVertex2f(h+0.1, 100);
        glVertex2f(h, 100);
    }

    glEnd();
}

void graphics_set_window_size(GraphicsHandle *handle, int width, int height)
{
    handle->width = width;
    handle->height = height;

    graphics_recalc_scale_vector(handle);
    graphics_update_camera(handle);
}



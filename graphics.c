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

    Matrix *matrix_data;

    double max;
    double min;
    double z_scale;
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
        rgb[0] = basic_table[_N_COLORS][0]; rgb[1] = basic_table[_N_COLORS][1]; rgb[2] = basic_table[_N_COLORS][2];
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
    handle->scale_vector[0] = (2.0f/handle->width) * 512; /* *zoom_factor */
    handle->scale_vector[1] = (2.0f/handle->height) * 512; /* *zoom_factor */
    handle->scale_vector[2] = 0.001f * 512; /* *zoom_factor */
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

void _graphics_render_block(double x, double y, double z, double dx, double dy)
{
    glVertex3f(x, y, z);
    glVertex3f(x + dx, y, z);
    glVertex3f(x + dx, y + dy, z);
    glVertex3f(x, y + dy, z);

    /* if draw blocks */
    glVertex3f(x, y, 0.0);
    glVertex3f(x + dx, y, 0.0);
    glVertex3f(x + dx, y, z);
    glVertex3f(x, y, z);

    glVertex3f(x + dx, y, 0);
    glVertex3f(x + dx, y + dy, 0);
    glVertex3f(x + dx, y + dy, z);
    glVertex3f(x + dx, y, z);

    glVertex3f(x + dx, y + dy, 0);
    glVertex3f(x, y + dy, 0);
    glVertex3f(x, y + dy, z);
    glVertex3f(x + dx, y + dy, z);

    glVertex3f(x, y + dy, 0);
    glVertex3f(x, y, 0);
    glVertex3f(x, y, z);
    glVertex3f(x, y + dy, z);
}

void graphics_render_matrix(GraphicsHandle *handle)
{
    double rgb[3];
    if (handle->matrix_data == NULL)
        return;
    MatrixIter iter;
    matrix_iter_init(handle->matrix_data, &iter);
    double x,y,z;
    double dx = 1.0f/handle->matrix_data->n_columns;
    double dy = 1.0f/handle->matrix_data->n_rows;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GEQUAL);

    glPolygonOffset(0.0, 0.0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBegin(GL_QUADS);

    for (matrix_iter_init(handle->matrix_data, &iter);
         matrix_iter_is_valid(handle->matrix_data, &iter);
         matrix_iter_next(handle->matrix_data, &iter)) {
        z = handle->matrix_data->chunks[iter.chunk][iter.offset];
        x = iter.column * dx - 0.5f;
        y = 0.5f - iter.row * dy - dy;

        color_gradient_rgb((z - handle->min)*handle->z_scale, rgb);
        z *= handle->z_scale;
        glColor3f(rgb[0], rgb[1], rgb[2]);

        _graphics_render_block(x, y, z, dx, dy);
    }

    glEnd();

    glPolygonOffset(-8.0, 5.0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBegin(GL_QUADS);

    for (matrix_iter_init(handle->matrix_data, &iter);
         matrix_iter_is_valid(handle->matrix_data, &iter);
         matrix_iter_next(handle->matrix_data, &iter)) {
        z = handle->matrix_data->chunks[iter.chunk][iter.offset];
        x = iter.column * dx - 0.5f;
        y = 0.5f - iter.row * dy - dy;

        z *= handle->z_scale;
        glColor3f(0.4, 0.4, 0.4);

        _graphics_render_block(x, y, z, dx, dy);
    }

    glEnd();
}

void graphics_render_grid(GraphicsHandle *handle)
{
    glBegin(GL_LINE_LOOP);
    glColor3f(0.0, 0.0, 0.0);
    glVertex3f(-0.5f, -0.5f, 0.0f);
    glVertex3f(0.5f, -0.5f, 0.0f);
    glVertex3f(0.5f, 0.5f, 0.0f);
    glVertex3f(-0.5f, 0.5f, 0.0f);
    glEnd();
#if 0
    glBegin(GL_LINES);
    /* x axis */
    glColor3f(1.0, 0.0, 0.0);
    glVertex3f(0.0, 0.0, 0.0);
    glVertex3f(1.0, 0.0, 0.0);

    /* y axis */
    glColor3f(0.0, 1.0, 0.0);
    glVertex3f(0.0, 0.0, 0.0);
    glVertex3f(0.0, 1.0, 0.0);

    /* z axis */
    glColor3f(0.0, 0.0, 1.0);
    glVertex3f(0.0, 0.0, 0.0);
    glVertex3f(0.0, 0.0, 1.0);
    glEnd();
#endif
}

void graphics_render(GraphicsHandle *handle)
{
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClearDepth(0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glViewport(0, 0, handle->width, handle->height);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixd(handle->projection_matrix);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    graphics_render_grid(handle);
    graphics_render_matrix(handle);

    glFinish();
}

void graphics_set_window_size(GraphicsHandle *handle, int width, int height)
{
    handle->width = width;
    handle->height = height;

    graphics_recalc_scale_vector(handle);
    graphics_update_camera(handle);
}

void graphics_set_matrix_data(GraphicsHandle *handle, Matrix *matrix)
{
    handle->matrix_data = matrix;
    /* determine max/min value and set range to scale */
    MatrixIter iter;
    double max, min;
    matrix_iter_init(matrix, &iter);
    if (matrix_iter_is_valid(matrix, &iter)) {
        max = min = matrix->chunks[iter.chunk][iter.offset];
    }
    matrix_iter_next(matrix, &iter);
    for ( ; matrix_iter_is_valid(matrix, &iter); matrix_iter_next(matrix, &iter)) {
        if (max < matrix->chunks[iter.chunk][iter.offset])
            max = matrix->chunks[iter.chunk][iter.offset];
        if (min > matrix->chunks[iter.chunk][iter.offset])
            min = matrix->chunks[iter.chunk][iter.offset];
    }

    handle->max = max;
    handle->min = min;
    handle->z_scale = min != max ? 1.0/(max-min) : 1.0;

    g_print("max/min/z_scale: %f/%f/%f\n", max, min, handle->z_scale);
    graphics_recalc_scale_vector(handle);
}


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
#include <cairo.h>
#include <pango/pangocairo.h>
#include <math.h>

#include "graphics.h"
#include "util-projection.h"
#include "util-png.h"

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
    double zoom_factor;
    gint8 zoom_level;

    double elevation;
    double azimuth;

    unsigned int overlay_tex_id;
    unsigned char *overlay_data;
    cairo_surface_t *overlay_surface;
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

void graphics_recalc_scale_vector(GraphicsHandle *handle)
{
    if (!handle->width || !handle->height)
        return;
    handle->scale_vector[0] = (2.0f/handle->width) * handle->zoom_factor;
    handle->scale_vector[1] = (2.0f/handle->height) * handle->zoom_factor;
    handle->scale_vector[2] = 0.001f * handle->zoom_factor;
    handle->scale_vector[3] = 1.0f;
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
    
    handle->azimuth = azimuth;
    handle->elevation = elevation;

    graphics_update_camera(handle);
}

void graphics_camera_zoom(GraphicsHandle *handle, gint steps)
{
    g_return_if_fail(handle != NULL);

    handle->zoom_level += steps;
    if (handle->zoom_level > 12)
        handle->zoom_level = 12;
    else if (handle->zoom_level < -12)
        handle->zoom_level = -12;

    handle->zoom_factor = handle->zoom_level >= -18 ? sqrt((double)(1 << (handle->zoom_level+18))) :
                                                    1.0f/sqrt(((double)(1 << (-handle->zoom_level-18))));

    graphics_recalc_scale_vector(handle);
    graphics_update_camera(handle);
}

void graphics_overlay_init(GraphicsHandle *handle)
{
    if (handle->overlay_surface)
        cairo_surface_destroy(handle->overlay_surface);
    if (handle->overlay_data)
        g_free(handle->overlay_data);
    if (handle->overlay_tex_id)
        glDeleteTextures(1, &handle->overlay_tex_id);

    glGenTextures(1, &handle->overlay_tex_id);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, handle->overlay_tex_id);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, handle->width, handle->height,
            0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);

    handle->overlay_data = g_malloc(4 * handle->width * handle->height);
    handle->overlay_surface = cairo_image_surface_create_for_data(handle->overlay_data,
            CAIRO_FORMAT_ARGB32, handle->width, handle->height, 4 * handle->width);
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

    handle->zoom_factor = 512.0f;
    handle->zoom_level = 0;

    return handle;
}

void graphics_cleanup(GraphicsHandle *handle)
{
    if (!handle)
        return;
    if (handle->overlay_surface)
        cairo_surface_destroy(handle->overlay_surface);
    if (handle->overlay_data)
        g_free(handle->overlay_data);
    if (handle->overlay_tex_id)
        glDeleteTextures(1, &handle->overlay_tex_id);
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

    glDisable(GL_TEXTURE_RECTANGLE_ARB);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GEQUAL);

    glPolygonOffset(0.0, 0.0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);

    for (matrix_iter_init(handle->matrix_data, &iter);
         matrix_iter_is_valid(handle->matrix_data, &iter);
         matrix_iter_next(handle->matrix_data, &iter)) {
        z = handle->matrix_data->chunks[iter.chunk][iter.offset];
        x = iter.column * dx - 0.5f;
        y = 0.5f - iter.row * dy - dy;

        color_gradient_rgb((z - handle->min)*handle->z_scale, rgb);
        z *= handle->z_scale;
        glColor4f(rgb[0], rgb[1], rgb[2], 1.0f);

        _graphics_render_block(x, y, z, dx, dy);
    }

    glEnd();

    glPolygonOffset(-8.0, 5.0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(0.5f);
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

void graphics_world_to_screen(GraphicsHandle *handle,
                              double wx, double wy, double wz,
                              double *sx, double *sy, double *sz)
{
    double vs[4];
    double vw[4] = { wx, wy, wz, 1.0f };
    util_vector_matrix_multiply(vw, handle->projection_matrix, vs);
    if (sx) *sx = (vs[0] + 1.0f) * 0.5f * handle->width;
    if (sy) *sy = (1.0f - vs[1]) * 0.5f * handle->height;
    if (sz) *sz = vs[2];
}

void graphics_render_overlay_tiks(GraphicsHandle *handle, cairo_t *cr)
{
    double wx = (handle->azimuth > 0 && handle->azimuth <= 180) ? -0.5f : 0.5f;
    double wy = (handle->azimuth > 90 && handle->azimuth <= 270) ? 0.5f : -0.5f;
    if (handle->elevation > 0) {
        wx = -wx;
        wy = -wy;
    }
    double z_floor = handle->min * handle->z_scale;

    /* which axis is left and should be shifted -width */
    int shift_axis_x = 0;
    /* if elevation > 0 also shift both labels by height */
    int shift_axis_y = 0;

    if ((handle->azimuth > 0 && handle->azimuth <= 90) ||
        (handle->azimuth > 180 && handle->azimuth <= 270)) {
        shift_axis_x = 1;
    }

    if (handle->elevation > 0) {
        shift_axis_x = 1 - shift_axis_x;
        shift_axis_y = 1;
    }

    double sx, sy;
    double shift_x, shift_y;
    gchar buf[128];

    double x;

    PangoLayout *layout;
    PangoFontDescription *desc;

    layout = pango_cairo_create_layout(cr);
    desc = pango_font_description_from_string("Sans 8");
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    PangoRectangle extents;

    cairo_save(cr);

    for (x = -0.5f; x <= 0.51f; x += 0.2f) {
        cairo_set_source_rgb(cr, 0.5f, 0.5f, 0.5f);
        sprintf(buf, "%d", (int)((x+0.5f)*handle->matrix_data->n_columns));
        pango_layout_set_markup(layout, buf, -1);
        pango_layout_get_pixel_extents(layout, NULL, &extents);
        graphics_world_to_screen(handle, x, wy, z_floor, &sx, &sy, NULL);

        shift_x = shift_axis_x == 0 ? -(double)extents.width - 1.0f : 1.0f;
        shift_y = (shift_axis_y == 1) ? -(double)extents.height : 0.0f;
        cairo_move_to(cr, sx + shift_x, sy + shift_y);
        pango_cairo_update_layout(cr, layout);
        pango_cairo_show_layout(cr, layout);

        cairo_set_source_rgb(cr, 0.5f, 0.5f, 0.5f);
        sprintf(buf, "%d", (int)((0.5f-x)*handle->matrix_data->n_rows));
        pango_layout_set_markup(layout, buf, -1);
        pango_layout_get_pixel_extents(layout, NULL, &extents);
        graphics_world_to_screen(handle, wx, x, z_floor, &sx, &sy, NULL);

        shift_x = shift_axis_x == 1 ? -(double)extents.width - 1.0f : 1.0f;
        shift_y = (shift_axis_y == 1) ? -(double)extents.height : 0.0f;
        cairo_move_to(cr, sx + shift_x, sy + shift_y);
        pango_cairo_update_layout(cr, layout);
        pango_cairo_show_layout(cr, layout);
    }

    g_object_unref(layout);

    cairo_restore(cr);
}

void graphics_render_overlay(GraphicsHandle *handle)
{
    cairo_t *cr = cairo_create(handle->overlay_surface);
    /* clear surface */
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_restore(cr);

/*    cairo_translate(cr, handle->width/2, handle->height/2);
    cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.7f);
    cairo_arc(cr, 0, 0, 50, 0, 2 * M_PI);
    cairo_fill(cr);*/

    PangoLayout *layout;
    PangoFontDescription *desc;

    cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.7f);
    layout = pango_cairo_create_layout(cr);

    desc = pango_font_description_from_string("Sans 8");
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    pango_layout_set_markup(layout, "Pango/Cairo enabled", -1);
    PangoRectangle extents;
    pango_layout_get_pixel_extents(layout, NULL, &extents);

    cairo_translate(cr, extents.x+1.0f, extents.y+1.0f);
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);

    g_object_unref(layout);

    graphics_render_overlay_tiks(handle, cr);


/* end preparing surface */
/* bring surface to texture */
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_RECTANGLE_ARB);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, handle->overlay_tex_id);
    glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, handle->width, handle->height,
            0, GL_BGRA, GL_UNSIGNED_BYTE, handle->overlay_data);

    glMatrixMode(GL_PROJECTION);
/*    glPushMatrix();*/
    glLoadIdentity();
    glOrtho(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, (GLfloat)handle->height);
    glVertex2f(0.0f, 0.0f);
    glTexCoord2f((GLfloat)handle->width, (GLfloat)handle->height);
    glVertex2f(1.0f, 0.0f);
    glTexCoord2f((GLfloat)handle->width, 0.0f);
    glVertex2f(1.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(0.0f, 1.0f);
    glEnd();
/*    glPopMatrix();*/

    cairo_destroy(cr);
}

void graphics_render_grid(GraphicsHandle *handle)
{
    glDisable(GL_TEXTURE_RECTANGLE_ARB);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GEQUAL);
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
    glColor3f(0.0, 0.0, 0.0);
    glVertex3f(-0.5f, -0.5f, 0.0f);
    glVertex3f(0.5f, -0.5f, 0.0f);
    glVertex3f(0.5f, 0.5f, 0.0f);
    glVertex3f(-0.5f, 0.5f, 0.0f);
    glEnd();
    
    double x, z;
    double wx = (handle->azimuth >= 0 && handle->azimuth <= 180) ? 0.5f : -0.5f;
    double wy = (handle->azimuth >= 90 && handle->azimuth <= 270) ? -0.5f : 0.5f;
    if (handle->elevation > 0) {
        wx = -wx;
        wy = -wy;
    }

    /* TODO: offset z to reasonable numbers < min (e.g. 2.17 -> 2.5) */
    double dz = (handle->max - handle->min) * handle->z_scale * 0.2f;
    double z_min = handle->min * handle->z_scale;
    double z_max = handle->max * handle->z_scale;
    double z_floor = z_min; /* elevation > 0 -> z_max ?? */

    glLineStipple(1, 0xaaaa);
    glEnable(GL_LINE_STIPPLE);
    glBegin(GL_LINES);

    glColor3f(0.2f, 0.2f, 0.2f);
    for (x = -0.5f; x <= 0.51f; x += 0.2f) {
        /* floor */
        glVertex3f(x, -0.5, z_floor);
        glVertex3f(x, 0.5, z_floor);

        glVertex3f(-0.5, x, z_floor);
        glVertex3f(0.5, x, z_floor);

        /* walls */
        glVertex3f(wx, x, z_min);
        glVertex3f(wx, x, z_max);

        glVertex3f(x, wy, z_min);
        glVertex3f(x, wy, z_max);

        for (z = z_min; z <= z_max+0.5f*dz; z += dz) {
            glVertex3f(wx, -0.5f, z);
            glVertex3f(wx, 0.5f, z);

            glVertex3f(-0.5f, wy, z);
            glVertex3f(0.5f, wy, z);
        }
    }

    glEnd();
/*    glDisable(GL_LINE_STIPPLE);*/

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
    glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
    glClearDepth(0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glViewport(0, 0, handle->width, handle->height);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixd(handle->projection_matrix);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    graphics_render_grid(handle);
    graphics_render_matrix(handle);
    graphics_render_overlay(handle);

    glFinish();
}

void graphics_set_window_size(GraphicsHandle *handle, int width, int height)
{
    handle->width = width;
    handle->height = height;

    graphics_recalc_scale_vector(handle);
    graphics_update_camera(handle);
    graphics_overlay_init(handle);
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

void graphics_save_buffer_to_file(GraphicsHandle *handle, const gchar *filename)
{
    g_return_if_fail(handle != NULL);
    g_return_if_fail(filename != NULL);

    guchar *buffer = g_malloc(handle->width * handle->height * 4);
    glReadPixels(0, 0, handle->width, handle->height,
            GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, buffer);

    util_write_to_png(filename, buffer, handle->width, handle->height);
    g_free(buffer);
}

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
#include "util-rectangle.h"
#include "util-colors.h"
#include "matrix-mesh.h"

#define ALMOST_EQUAL(a,b) ((a)-(b) < 0.001f && (b)-(a) < 0.001f)

#ifdef DEBUG
void print_matrix(double *m)
{
    int i;
    for (i = 0; i < 16; i+=4) {
        g_print("%f %f %f %f\n", m[i+0], m[i+1], m[i+2], m[i+3]);
    }
}
#endif

struct _GraphicsHandle {
    int width;
    int height;

    double projection_matrix[16];
    double projection_matrix_inv[16];
    double rotation_matrix[16];
    double rotation_matrix_inv[16];
    double scale_vector[4];
    double translation_vector[3];
    double translation_x[3];
    double translation_y[3];

    double tmp_rotation_matrix[16];
    double tmp_rotation_matrix_inv[16];
    double tmp_translation_vector[3];

    double start_screen_pos[2];

    guint32 inv_projection_valid : 1;
    guint32 in_tmp_rotation : 1;
    guint32 in_tmp_translation : 1;
    guint32 display_list_initialized : 1;
    guint32 display_list_valid : 1;

    Matrix *matrix_data;
    double alpha_channel;

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

    UtilRectangle render_area;

    GLuint display_list;
};

void graphics_get_far_planes(GraphicsHandle *handle, double *planes)
{
    planes[0] = handle->projection_matrix[2] >= 0 ? -0.5f : 0.5f;
    planes[1] = handle->projection_matrix[6] >= 0 ? -0.5f : 0.5f;
    planes[2] = handle->projection_matrix[10] >= 0 ? handle->min * handle->z_scale : handle->max * handle->z_scale;
}

void graphics_recalc_scale_vector(GraphicsHandle *handle)
{
    if (!handle->width || !handle->height)
        return;
    handle->scale_vector[0] = (2.0f/handle->width) * handle->zoom_factor;
    handle->scale_vector[1] = (2.0f/handle->height) * handle->zoom_factor;
    handle->scale_vector[2] = 0.0001f * handle->zoom_factor;
    handle->scale_vector[3] = 1.0f;
}

void graphics_calc_inverse_projection(GraphicsHandle *handle)
{
    double vec[16];
    if (handle->inv_projection_valid == 0) {
        util_matrix_identify(handle->projection_matrix_inv);
        /* inverse of scale matrix */
        vec[0] = 1.0f/handle->scale_vector[0];
        vec[1] = 1.0f/handle->scale_vector[1];
        vec[2] = 1.0f/handle->scale_vector[2];
        vec[3] = 1.0f/handle->scale_vector[3];
        util_scale_matrix(handle->projection_matrix_inv, vec);

        /* inverse of rotation */
        if (handle->in_tmp_rotation) {
            util_matrix_multiply(handle->tmp_rotation_matrix_inv,
                                 handle->projection_matrix_inv,
                                 handle->projection_matrix_inv);
        }
        util_matrix_multiply(handle->rotation_matrix_inv,
                             handle->projection_matrix_inv,
                             handle->projection_matrix_inv);

        /* inverse of translation */
        if (handle->in_tmp_translation) {
            vec[0] = -handle->tmp_translation_vector[0];
            vec[1] = -handle->tmp_translation_vector[1];
            vec[2] = -handle->tmp_translation_vector[2];
            util_translate_matrix(handle->projection_matrix_inv, vec);
        }
        vec[0] = -handle->translation_vector[0];
        vec[1] = -handle->translation_vector[1];
        vec[2] = -handle->translation_vector[2];
        util_translate_matrix(handle->projection_matrix_inv, vec);

        handle->inv_projection_valid = 1;
    }
}

void graphics_calc_screen_vectors(GraphicsHandle *handle)
{
    graphics_calc_inverse_projection(handle);
    double *m = handle->projection_matrix_inv;

    /* map one pixel in x/y direction to unit cube [-1,1]^3 */
    double sx = 2.0f/handle->width;
    double sy = 2.0f/handle->height;

    /* “calculate” inverse projection of the unit vectors screen x, screen y */
    handle->translation_x[0] = m[0] * sx;
    handle->translation_x[1] = m[1] * sx;
    handle->translation_x[2] = m[2] * sx;

    handle->translation_y[0] = m[4] * sy;
    handle->translation_y[1] = m[5] * sy;
    handle->translation_y[2] = m[6] * sy;
}

void graphics_update_camera(GraphicsHandle *handle)
{
    if (!handle->width || !handle->height)
        return;
    double angles[3];
    util_matrix_identify(handle->projection_matrix);
    util_translate_matrix(handle->projection_matrix, handle->translation_vector);
    if (handle->in_tmp_translation)
        util_translate_matrix(handle->projection_matrix, handle->tmp_translation_vector);

    util_matrix_multiply(handle->rotation_matrix, handle->projection_matrix, handle->projection_matrix);
    if (handle->in_tmp_rotation)
        util_matrix_multiply(handle->tmp_rotation_matrix, 
                             handle->projection_matrix,
                             handle->projection_matrix);

    util_rotation_matrix_get_eulerian_angels(handle->projection_matrix, angles);
    handle->azimuth = angles[0];
    handle->elevation = angles[1];
   /* handle->tilt = angles[2];*/

    util_scale_matrix(handle->projection_matrix, handle->scale_vector);

    handle->inv_projection_valid = 0;

    graphics_calc_screen_vectors(handle);
}

void graphics_set_camera(GraphicsHandle *handle, double azimuth, double elevation, double tilt)
{
    g_return_if_fail(handle != NULL);

    util_get_rotation_matrix_from_angles(handle->rotation_matrix, azimuth, elevation, tilt);

    /* set inverse rotation */
    memcpy(handle->rotation_matrix_inv, handle->rotation_matrix, sizeof(double) * 16);
    util_transpose_matrix(handle->rotation_matrix_inv);

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

void graphics_camera_move_start(GraphicsHandle *handle, double x, double y)
{
    g_return_if_fail(handle != NULL);

    handle->in_tmp_translation = 1;
    handle->start_screen_pos[0] = x;
    handle->start_screen_pos[1] = y;

    handle->tmp_translation_vector[0] = 0.0f;
    handle->tmp_translation_vector[1] = 0.0f;
    handle->tmp_translation_vector[2] = 0.0f;
}

void graphics_camera_move_update(GraphicsHandle *handle, double x, double y)
{
    g_return_if_fail(handle != NULL);

    if (!handle->in_tmp_translation)
        return;

    double dx = x - handle->start_screen_pos[0];
    double dy = y - handle->start_screen_pos[1];

    /* update translation; y is given in other direction then in our coordinate system */
    handle->tmp_translation_vector[0] =   dx * handle->translation_x[0]
                                        - dy * handle->translation_y[0];
    handle->tmp_translation_vector[1] =   dx * handle->translation_x[1]
                                        - dy * handle->translation_y[1];
    handle->tmp_translation_vector[2] =   dx * handle->translation_x[2]
                                        - dy * handle->translation_y[2];

    graphics_update_camera(handle);
}

void graphics_camera_move_finish(GraphicsHandle *handle, double x, double y)
{
    g_return_if_fail(handle != NULL);

    if (!handle->in_tmp_translation)
        return;

    double dx = x - handle->start_screen_pos[0];
    double dy = y - handle->start_screen_pos[1];

    handle->translation_vector[0] += dx * handle->translation_x[0]
                                   - dy * handle->translation_y[0];
    handle->translation_vector[1] += dx * handle->translation_x[1]
                                   - dy * handle->translation_y[1];
    handle->translation_vector[2] += dx * handle->translation_x[2]
                                   - dy * handle->translation_y[2];

    handle->in_tmp_translation = 0;

    graphics_update_camera(handle);
}

void graphics_arcball_convert(GraphicsHandle *handle, double x, double y, double *v, ArcBallRestriction rst)
{
    /* map a screen point to a unit ball */
/*    double radius = handle->width >= handle->height ? 0.5f * handle->height : 0.5f * handle->width;*/
    v[0] = (x - 0.5f * handle->width) / (0.5f * handle->width);/*radius;*/
    v[1] = (0.5f * handle->height - y) / (0.5f * handle->height);/*radius;*/
    if (rst == ArcBallRestrictionHorizontal)
        v[0] = 0.0f;
    else if (rst == ArcBallRestrictionVertical)
        v[1] = 0.0f;
    double len = v[0] * v[0] + v[1] * v[1];
    if (len <= 1.0f) {
        v[2] = sqrt(1.0f - len);
    }
    else {
        len = sqrt(len);
        v[0] /= len;
        v[1] /= len;
        v[2] = 0.0f;
    }
}

void graphics_camera_arcball_rotate_start(GraphicsHandle *handle, double x, double y)
{
    g_return_if_fail(handle != NULL);

    handle->in_tmp_rotation = 1;
    util_matrix_identify(handle->tmp_rotation_matrix);
    util_matrix_identify(handle->tmp_rotation_matrix_inv);

    handle->start_screen_pos[0] = x;
    handle->start_screen_pos[1] = y;
}

void graphics_camera_arcball_rotate_update(GraphicsHandle *handle, double x, double y, ArcBallRestriction rst)
{
    g_return_if_fail(handle != NULL);

    if (!handle->in_tmp_rotation)
        return;

    if (ALMOST_EQUAL(handle->start_screen_pos[0], x) && 
        ALMOST_EQUAL(handle->start_screen_pos[1], y)) {
        util_matrix_identify(handle->tmp_rotation_matrix);
        util_matrix_identify(handle->tmp_rotation_matrix_inv);
        return;
    }

    double p0[3], p1[3];
    graphics_arcball_convert(handle, handle->start_screen_pos[0], handle->start_screen_pos[1], p0, rst);
    graphics_arcball_convert(handle, x, y, p1, rst);

    /* get angle between start point on unit ball and current point, as well as the rotation axis */

    double dotprod = p0[0] * p1[0] + p0[1] * p1[1] + p0[2] * p1[2];
    double angle = acos(dotprod) * 180.0f * M_1_PI;

    double axis[3] = {
        p0[1] * p1[2] - p0[2] * p1[1],
        p0[2] * p1[0] - p0[0] * p1[2],
        p0[0] * p1[1] - p0[1] * p1[0]
    };

    util_get_rotation_matrix(handle->tmp_rotation_matrix, angle, UTIL_AXIS_CUSTOM, axis);
    memcpy(handle->tmp_rotation_matrix_inv, handle->tmp_rotation_matrix, sizeof(double) * 16);
    util_transpose_matrix(handle->tmp_rotation_matrix_inv);

    graphics_update_camera(handle);
}

void graphics_camera_arcball_rotate_finish(GraphicsHandle *handle, double x, double y, ArcBallRestriction rst)
{
    g_return_if_fail(handle != NULL);

    if (!handle->in_tmp_rotation)
        return;

    if (ALMOST_EQUAL(handle->start_screen_pos[0], x) &&
        ALMOST_EQUAL(handle->start_screen_pos[1], y)) {
        handle->in_tmp_rotation = 0;
        return;
    }

    handle->in_tmp_rotation = 0;

    double p0[3], p1[3];
    graphics_arcball_convert(handle, handle->start_screen_pos[0], handle->start_screen_pos[1], p0, rst);
    graphics_arcball_convert(handle, x, y, p1, rst);

    double dotprod = p0[0] * p1[0] + p0[1] * p1[1] + p0[2] * p1[2];
    double angle = acos(dotprod) * 180.0f * M_1_PI;
    double axis[3] = {
        p0[1] * p1[2] - p0[2] * p1[1],
        p0[2] * p1[0] - p0[0] * p1[2],
        p0[0] * p1[1] - p0[1] * p1[0]
    };

    util_get_rotation_matrix(handle->tmp_rotation_matrix, angle, UTIL_AXIS_CUSTOM, axis);
    memcpy(handle->tmp_rotation_matrix_inv, handle->tmp_rotation_matrix, sizeof(double) * 16);
    util_transpose_matrix(handle->tmp_rotation_matrix_inv);

    util_matrix_multiply(handle->tmp_rotation_matrix, handle->rotation_matrix, handle->rotation_matrix);
    util_matrix_multiply(handle->rotation_matrix_inv, handle->tmp_rotation_matrix_inv, handle->rotation_matrix_inv);

    handle->in_tmp_rotation = 0;

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

    handle->translation_vector[0] = 0.0f;
    handle->translation_vector[1] = 0.0f;
    handle->translation_vector[2] = 0.0f;

    handle->zoom_factor = 512.0f;
    handle->zoom_level = 0;

    handle->alpha_channel = 1.0f;

    handle->display_list_valid = 0;
    handle->display_list_initialized = 0;

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
    if (handle->display_list_initialized)
        glDeleteLists(handle->display_list, 1);
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
    if (handle->matrix_data == NULL)
        return;

    if (handle->display_list_initialized == 0) {
        handle->display_list = glGenLists(1);
        handle->display_list_initialized = 1;
    }
    if (handle->display_list_valid == 1) {
        glCallList(handle->display_list);
        return;
    }

    MatrixMesh *mesh = matrix_mesh_new();
    matrix_mesh_set_alpha_channel(mesh, handle->alpha_channel);
    matrix_mesh_set_matrix(mesh, handle->matrix_data);
    MatrixMeshIter fiter;
    MatrixMeshFace *face;

    int j;

    glNewList(handle->display_list, GL_COMPILE_AND_EXECUTE);

    glDisable(GL_TEXTURE_RECTANGLE_ARB);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GEQUAL);

    glPolygonOffset(0.0, 0.0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);

    for (matrix_mesh_iter_init(mesh, &fiter);
         matrix_mesh_iter_is_valid(mesh, &fiter);
         matrix_mesh_iter_next(mesh, &fiter)) {
        face = &mesh->chunk_faces[fiter.chunk][fiter.offset];
        glColor4f(face->color_rgba[0], face->color_rgba[1], face->color_rgba[2], face->color_rgba[3]);

        for (j = 0; j < 4; ++j)
            glVertex3f(face->vertices[j][0], face->vertices[j][1], face->vertices[j][2]);
    }


    glEnd();

    glPolygonOffset(-8.0, 5.0);
    glDisable(GL_LINE_STIPPLE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(0.5f);
    glBegin(GL_QUADS);

    for (matrix_mesh_iter_init(mesh, &fiter);
         matrix_mesh_iter_is_valid(mesh, &fiter);
         matrix_mesh_iter_next(mesh, &fiter)) {
        face = &mesh->chunk_faces[fiter.chunk][fiter.offset];
        glColor3f(0.4, 0.4, 0.4);

        for (j = 0; j < 4; ++j)
            glVertex3f(face->vertices[j][0], face->vertices[j][1], face->vertices[j][2]);
    }

    glEnd();

    glEndList();
    handle->display_list_valid = 1;

    matrix_mesh_free(mesh);
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

void graphics_screen_to_world(GraphicsHandle *handle,
                              double sx, double sy, double sz,
                              double *wx, double *wy, double *wz)
{
    double vs[4] = { 2.0f*sx/handle->width-1.0f,
                     1.0f - 2.0f*sy/handle->height, sz, 1.0f };
    double vw[4];
    graphics_calc_inverse_projection(handle);
    util_vector_matrix_multiply(vs, handle->projection_matrix_inv, vw);
    if (wx) *wx = vw[0];
    if (wy) *wy = vw[1];
    if (wz) *wz = vw[2];
}

void graphics_map_bounding_box(GraphicsHandle *handle, UtilRectangle *bounding_box)
{
    double xr[2];
    double yr[2];

    double z_min = handle->min * handle->z_scale;
    double z_max = handle->max * handle->z_scale;

    double sx, sy;
    graphics_world_to_screen(handle, -0.5f, -0.5f, z_min, &sx, &sy, NULL);
    xr[0] = xr[1] = sx;
    yr[0] = yr[1] = sy;

#define EXPAND_BOX(wx,wy,wz) do {\
    graphics_world_to_screen(handle, (wx), (wy), (wz), &sx, &sy, NULL);\
    if (sx < xr[0]) xr[0] = sx;\
    if (sx > xr[1]) xr[1] = sx;\
    if (sy < yr[0]) yr[0] = sy;\
    if (sy > yr[1]) yr[1] = sy;\
    } while (0)

    EXPAND_BOX(-0.5f, 0.5f, z_min);
    EXPAND_BOX(0.5f, 0.5f, z_min);
    EXPAND_BOX(0.5f, -0.5f, z_min);

    EXPAND_BOX(-0.5f, -0.5f, z_max);
    EXPAND_BOX(-0.5f, 0.5f, z_max);
    EXPAND_BOX(0.5f, 0.5f, z_max);
    EXPAND_BOX(0.5f, -0.5f, z_max);

#undef EXPAND_BOX

    if (bounding_box) {
        bounding_box->x = xr[0];
        bounding_box->y = yr[0];
        bounding_box->width = (xr[1]-xr[0]+1.0f);
        bounding_box->height = (yr[1]-yr[0]+1.0f);
    }
}

void graphics_render_overlay_tiks(GraphicsHandle *handle, cairo_t *cr, UtilRectangle *bounding_box,
                                  GraphicsTiksCallback callback, gpointer userdata)
{
    double far_planes[3];
    graphics_get_far_planes(handle, far_planes);
    double wx = - far_planes[0];
    double wy = - far_planes[1];
    double z_floor = far_planes[2];

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

    PangoRectangle extents;
    gboolean initialized = FALSE;
    double xr[2];
    double yr[2];

    if (!callback) {
        layout = pango_cairo_create_layout(cr);
        desc = pango_font_description_from_string("Sans 8");
        pango_layout_set_font_description(layout, desc);
        pango_font_description_free(desc);

        cairo_save(cr);

        cairo_set_source_rgb(cr, 0.5f, 0.5f, 0.5f);
    }

    for (x = -0.5f; x <= 0.51f; x += 0.2f) {
#define UPDATE_RANGE do {\
    if (sx + shift_x < xr[0]) xr[0] = sx + shift_x;\
    if (sx + shift_x + extents.width > xr[1]) xr[1] = sx + shift_x + extents.width;\
    if (sy + shift_y < yr[0]) yr[0] = sy + shift_y;\
    if (sy + shift_y + extents.height > yr[1]) yr[1] = sy + shift_y + extents.height;\
    } while(0)

#define UPDATE_RANGE_SIMPLE do {\
    if (sx < xr[0]) xr[0] = sx;\
    if (sx > xr[1]) xr[1] = sx;\
    if (sy < yr[0]) yr[0] = sy;\
    if (sy > yr[1]) yr[1] = sy;\
    } while (0)

        sprintf(buf, "%d", (int)((x+0.5f)*handle->matrix_data->n_columns));
        graphics_world_to_screen(handle, x, wy, z_floor, &sx, &sy, NULL);

        if (!callback) {
            pango_layout_set_markup(layout, buf, -1);
            pango_layout_get_pixel_extents(layout, NULL, &extents);
            shift_x = shift_axis_x == 0 ? -(double)extents.width - 1.0f : 1.0f;
            shift_y = (shift_axis_y == 1) ? -(double)extents.height : 0.0f;
            cairo_move_to(cr, sx + shift_x, sy + shift_y);
            pango_cairo_update_layout(cr, layout);
            pango_cairo_show_layout(cr, layout);

            if (!initialized) {
                xr[0] = sx + shift_x;
                xr[1] = xr[0] + (double)extents.width;
                yr[0] = sy + shift_y;
                yr[1] = yr[1] + (double)extents.height;
                initialized = TRUE;
            }
            UPDATE_RANGE;
        }
        else {
            if (!initialized) {
                xr[0] = xr[1] = sx;
                yr[0] = yr[1] = sy;
                initialized = TRUE;
            }
            UPDATE_RANGE_SIMPLE;
            callback(sx + (shift_axis_x ? 1.0f : -1.0f), sy,
                     (shift_axis_x ? TiksAlignLeft : TiksAlignRight) |
                     (shift_axis_y ? TiksAlignTop : TiksAlignBottom),
                     buf, userdata);
        }

        sprintf(buf, "%d", (int)((0.5f-x)*handle->matrix_data->n_rows));
        graphics_world_to_screen(handle, wx, x, z_floor, &sx, &sy, NULL);

        if (!callback) {
            pango_layout_set_markup(layout, buf, -1);
            pango_layout_get_pixel_extents(layout, NULL, &extents);
            shift_x = shift_axis_x == 1 ? -(double)extents.width - 1.0f : 1.0f;
            shift_y = (shift_axis_y == 1) ? -(double)extents.height : 0.0f;
            cairo_move_to(cr, sx + shift_x, sy + shift_y);
            pango_cairo_update_layout(cr, layout);
            pango_cairo_show_layout(cr, layout);
            UPDATE_RANGE;
        }
        else {
            UPDATE_RANGE_SIMPLE;
            callback(sx + (shift_axis_x ? -1.0f : 1.0f), sy,
                     (shift_axis_x ? TiksAlignRight : TiksAlignLeft) |
                     (shift_axis_y ? TiksAlignTop : TiksAlignBottom),
                     buf, userdata);
        }


#undef UPDATE_RANGE
#undef UPDATE_RANGE_SIMPLE
    }

    if (!callback) {
        g_object_unref(layout);

        cairo_restore(cr);
    }

    if (bounding_box) {
        bounding_box->x = xr[0];
        bounding_box->y = yr[0];
        bounding_box->width = (xr[1] - xr[0] + 1.0f);
        bounding_box->height = (yr[1] - yr[0] + 1.0f);
    }
}

void graphics_render_overlay(GraphicsHandle *handle, UtilRectangle *bounding_box,
                             GraphicsTiksCallback callback, gpointer userdata)
{
    cairo_t *cr = cairo_create(handle->overlay_surface);
    /* clear surface */
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_restore(cr);

    UtilRectangle box_tiks;
    graphics_render_overlay_tiks(handle, cr, &box_tiks, callback, userdata);

    if (bounding_box) *bounding_box = box_tiks;

#ifdef DEBUG
    cairo_set_source_rgb(cr, 0.0f, 1.0f, 0.0f);
    cairo_rectangle(cr, box_tiks.x, box_tiks.y, box_tiks.width, box_tiks.height);
    cairo_stroke(cr);

    UtilRectangle box_render;
    graphics_map_bounding_box(handle, &box_render);
    cairo_set_source_rgb(cr, 1.0f, 0.0f, 0.0f);
    cairo_rectangle(cr, box_render.x, box_render.y, box_render.width, box_render.height);
    cairo_stroke(cr);

    util_rectangle_bounds(&box_render, &box_render, &box_tiks);
    UtilRectangle crop = { 0.0f, 0.0f, handle->width, handle->height };
    util_rectangle_crop(&box_render, &crop);
    cairo_set_source_rgb(cr, 0.0f, 0.0f, 1.0f);
    cairo_rectangle(cr, box_render.x, box_render.y, box_render.width, box_render.height);
    cairo_stroke(cr);
#endif


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

#if DEBUG
    glLineWidth(4.0f);
    glBegin(GL_LINES);
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(-0.5f, -0.5f, handle->min * handle->z_scale);
    glVertex3f(10 * handle->translation_x[0]-0.5f, 10 * handle->translation_x[1]-0.5f, 10 * handle->translation_x[2] + handle->min * handle->z_scale);
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-0.5f, -0.5f, handle->min * handle->z_scale);
    glVertex3f(10 * handle->translation_y[0] - 0.5f, 10 * handle->translation_y[1]-0.5f, 10 * handle->translation_y[2] + handle->min * handle->z_scale);
    glEnd();
#endif
    
    double x, z;
    double far_planes[3];
    graphics_get_far_planes(handle, far_planes);
    double wx = far_planes[0];
    double wy = far_planes[1];

    /* TODO: offset z to reasonable numbers < min (e.g. 2.17 -> 2.5) */
    double dz = (handle->max - handle->min) * handle->z_scale * 0.2f;
    double z_min = handle->min * handle->z_scale;
    double z_max = handle->max * handle->z_scale;
    double z_floor = far_planes[2]; /* elevation > 0 -> z_max ?? */

    glLineStipple(1, 0xaaaa);
    glLineWidth(1.0f);
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

void graphics_render(GraphicsHandle *handle, GraphicsTiksCallback callback, gpointer userdata)
{
    glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
    glClearDepth(0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glViewport(0, 0, handle->width, handle->height);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixd(handle->projection_matrix);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    UtilRectangle overlay_box;

    graphics_render_grid(handle);
    graphics_render_matrix(handle);
#if 0
    DEBUG
    double x[2], y[2], z[2];

    graphics_calc_inverse_projection(handle);

    graphics_screen_to_world(handle, 100, 100, 0, &x[0], &y[0], &z[0]);
    graphics_screen_to_world(handle, 110, 110, 0.1, &x[1], &y[1], &z[1]);
    glPointSize(20.0f);
    glBegin(GL_POINTS);
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(x[0], y[0], z[0]);
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(x[1], y[1], z[1]);
    glEnd();
    fprintf(stderr, "s[0] (%f,%f,%f)\n", x[0], y[0], z[0]);
    fprintf(stderr, "s[1] (%f,%f,%f)\n", x[1], y[1], z[1]);

#endif
    graphics_render_overlay(handle, &overlay_box, callback, userdata);

    glFinish();

    graphics_map_bounding_box(handle, &handle->render_area);
    util_rectangle_bounds(&handle->render_area, &handle->render_area, &overlay_box);
    UtilRectangle crop = { 0.0f, 0.0f, handle->width, handle->height };
    util_rectangle_crop(&handle->render_area, &crop);
}

void graphics_set_window_size(GraphicsHandle *handle, int width, int height)
{
    handle->width = width;
    handle->height = height;

    graphics_recalc_scale_vector(handle);
    graphics_update_camera(handle);
    graphics_overlay_init(handle);
}

void graphics_update_matrix_data(GraphicsHandle *handle)
{
    /* determine max/min value and set range to scale */
    Matrix *matrix = handle->matrix_data;
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

    handle->display_list_valid = 0;
}

void graphics_set_matrix_data(GraphicsHandle *handle, Matrix *matrix)
{
    handle->matrix_data = matrix;

    graphics_update_matrix_data(handle);
}

void graphics_set_alpha_channel(GraphicsHandle *handle, double alpha_channel)
{
    g_return_if_fail(handle != NULL);

    handle->alpha_channel = alpha_channel;
}

void graphics_save_buffer_to_file(GraphicsHandle *handle, const gchar *filename)
{
    g_return_if_fail(handle != NULL);
    g_return_if_fail(filename != NULL);

    /*
    glReadPixels(0, 0, handle->width, handle->height,
            GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, buffer);*/
    int offset[2] = { floor(handle->render_area.x), floor(handle->render_area.y) };
    int size[2] = { ceil(handle->render_area.width), ceil(handle->render_area.height) };
    if (offset[0] + size[0] > handle->width)
        size[0] = handle->width - offset[0];
    if (offset[1] + size[1] > handle->height)
        size[1] = handle->height - offset[1];
    offset[1] = handle->height - offset[1] - size[1];

    guchar *buffer = g_malloc(size[0] * size[1] * 4);
    glReadPixels(offset[0], offset[1], size[0], size[1],
            GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, buffer);

    util_write_to_png(filename, buffer, size[0], size[1]);
    g_free(buffer);
}

void graphics_get_render_area(GraphicsHandle *handle, UtilRectangle *render_area)
{
    g_return_if_fail(handle != NULL);
    g_return_if_fail(render_area != NULL);

    *render_area = handle->render_area;
}

void graphics_get_rotation(GraphicsHandle *handle, double *rotation_matrix)
{
    g_return_if_fail(handle != NULL);
    g_return_if_fail(rotation_matrix != NULL);

    memcpy(rotation_matrix, handle->rotation_matrix, 16 * sizeof(double));
}

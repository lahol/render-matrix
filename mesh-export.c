#include "mesh-export.h"
#include "util-projection.h"
#include "util-rectangle.h"

#include <cairo.h>
#include <cairo-svg.h>
#include <cairo-pdf.h>

struct SVGFace {
    double vertices[4][3];
    double color[4];
    UtilRectangle bounding_box;
    double zvalue;
};

ExportFileType mesh_export_get_type_from_filename(const gchar *filename)
{
    if (g_str_has_suffix(filename, ".pdf"))
        return ExportFileTypePDF;
    if (g_str_has_suffix(filename, ".svg"))
        return ExportFileTypeSVG;
    if (g_str_has_suffix(filename, ".png"))
        return ExportFileTypePNG;

    return ExportFileTypeUnknown;
}

void mesh_export_world_to_screen(double *projection, double *wv, double *sv)
{
    double vs[4];
    double vw[4] = { wv[0], wv[1], wv[2], 1.0f };
    util_vector_matrix_multiply(vw, projection, vs);

    sv[0] = (vs[0] + 1.0f) * 0.5f * 10.0f * 72;
    sv[1] = (1.0f - vs[1]) * 0.5f * 10.0f * 72;
    sv[2] = vs[2];
#ifdef DEBUG
    fprintf(stderr, "(%.2f, %.2f, %.2f) -> (%.2f, %.2f, %.2f) [%.2f, %.2f, %.2f]\n",
            wv[0], wv[1], wv[2], sv[0], sv[1], sv[2], vs[0], vs[1], vs[2]);
#endif
}

gint mesh_export_sort_zlevel(struct SVGFace *a, struct SVGFace *b)
{
#if 0
    DEBUG
    int i;
    double min_y_a = a->vertices[0][2];
    double min_y_b = b->vertices[0][2];
    for (i = 1; i < 4; ++i) {
        if (min_y_a > a->vertices[i][2])
            min_y_a = a->vertices[i][2];
        if (min_y_b > b->vertices[i][2])
            min_y_b = b->vertices[i][2];
    }

    if (min_y_a < 2200.0f && min_y_b < 2200.0f) {
        fprintf(stderr, "min_y (%.2f, %.2f), z: cmp(%f, %f): %d\n",
                min_y_a, min_y_b, a->zlevel, b->zlevel, a->zlevel < b->zlevel ? -1 : (a->zlevel > b->zlevel ? 1 : 0));
    }
    
#endif
/*    fprintf(stderr, "a(%.2f, %.2f), (%.2f x %.2f)\nb(%.2f,%2f), (%.2f x %.2f)\n",
            a->bounding_box.x, a->bounding_box.y, a->bounding_box.width, a->bounding_box.height,
            b->bounding_box.x, b->bounding_box.y, b->bounding_box.width, b->bounding_box.height);*/
/*    if (!util_do_rectangles_overlap(&(a->bounding_box), &(b->bounding_box))) {
        return 0;
    }*/
    if (a->zvalue < b->zvalue)
        return -1;
    if (a->zvalue > b->zvalue)
        return 1;
#if 0
    /* project points of a on face of b (is more or less rectangle)
     * get coordinates in terms of v0->v1; v0->v3 if in [0,1] -> interpolate z value and compare
     * if no such point is found they do not overlap */
    double u1[2], u2[2];
    u1[0] = b->vertices[1][0] - b->vertices[0][0];
    u1[1] = b->vertices[1][1] - b->vertices[0][1];
    u2[0] = b->vertices[3][0] - b->vertices[0][0];
    u2[1] = b->vertices[3][1] - b->vertices[0][1];

    double det = u1[0]*u2[1] - u1[1]*u2[0];
    if (det == 0)
        return 0;

    double lambda[2];
    guint8 j;
    double z;
    for (j = 0; j < 4; ++j) {
        lambda[0] = (u2[1] * a->vertices[j][0] - u2[0] * a->vertices[j][1]) / det;
        lambda[1] = (u1[0] * a->vertices[j][1] - u1[1] * a->vertices[j][0]) / det;
    /*    fprintf(stderr, "lambda: %f, %f\n", lambda[0], lambda[1]);
        if (lambda[0] >= 0.0f && lambda[0] <= 1.0f &&
            lambda[1] >= 0.0f && lambda[1] <= 1.0f) {*/
            /* point on face; interpolate zvalue and compare */
            z = b->vertices[0][2] + lambda[0] * (b->vertices[1][2] - b->vertices[0][2])
                                  + lambda[1] * (b->vertices[3][2] - b->vertices[0][2]);
            fprintf(stderr, "a(%d, z): %f, pb(z): %f\n", j, a->vertices[j][2], z);
            if (a->vertices[j][2] > z)
                return 1;
            else if (a->vertices[j][2] < z)
                return -1;
/*        }*/
    }
#endif
    return 0;
}

void _mesh_face_update_bounding_box(struct SVGFace *face)
{
    double xr[2], yr[2];
    guint8 j;

    xr[0] = xr[1] = face->vertices[0][0];
    yr[0] = yr[1] = face->vertices[0][1];

    for (j = 1; j < 4; ++j) {
        if (xr[0] > face->vertices[j][0])
            xr[0] = face->vertices[j][0];
        if (xr[1] < face->vertices[j][0])
            xr[1] = face->vertices[j][0];
        if (yr[0] > face->vertices[j][1])
            yr[0] = face->vertices[j][1];
        if (yr[1] < face->vertices[j][1])
            yr[1] = face->vertices[j][1];
    }

    face->bounding_box.x = xr[0];
    face->bounding_box.y = yr[0];
    face->bounding_box.width = xr[1] - xr[0];
    face->bounding_box.height = yr[1] - yr[0];
}

GList *mesh_export_generate_faces(MatrixMesh *mesh, double *projection, UtilRectangle *bounding_box)
{
    MatrixMeshIter iter;
    GList *faces = NULL;
    struct SVGFace *face;
    guint8 j;
    double xr[2], yr[2];
    double zrefpoint[3];
    double zrefproj[3];
    gboolean bd_initialized = FALSE;

#ifdef DEBUG
    for (j=0; j < 16; j+=4)
        fprintf(stderr, "proj: [%.3f %.3f %.3f %.3f]\n", projection[j], projection[j+1], projection[j+2], projection[j+3]);
#endif

    for (matrix_mesh_iter_init(mesh, &iter);
         matrix_mesh_iter_is_valid(mesh, &iter);
         matrix_mesh_iter_next(mesh, &iter)) {

        /* FIXME: only pack pointers in list and use chunk saving? */
        face = g_malloc0(sizeof(struct SVGFace));

        for (j = 0; j < 4; ++j) {
            mesh_export_world_to_screen(projection, mesh->chunk_faces[iter.chunk][iter.offset].vertices[j], face->vertices[j]);
        }
/*        _mesh_face_update_bounding_box(face);*/

        /* FIXME: when we already have the bounding box, use util_rectangle_bounds to expand current box by this rectangle */
        if (G_UNLIKELY(!bd_initialized)) {
            xr[0] = xr[1] = face->vertices[0][0];
            yr[0] = yr[1] = face->vertices[0][1];
            bd_initialized = TRUE;
        }

        for (j = 0; j < 4; ++j) {
            if (face->vertices[j][0] < xr[0])
                xr[0] = face->vertices[j][0];
            if (face->vertices[j][0] > xr[1])
                xr[1] = face->vertices[j][0];
            if (face->vertices[j][1] < yr[0])
                yr[0] = face->vertices[j][1];
            if (face->vertices[j][1] > yr[1])
                yr[1] = face->vertices[j][1];
        }
    

        face->color[0] = mesh->chunk_faces[iter.chunk][iter.offset].color_rgba[0];
        face->color[1] = mesh->chunk_faces[iter.chunk][iter.offset].color_rgba[1];
        face->color[2] = mesh->chunk_faces[iter.chunk][iter.offset].color_rgba[2];
        face->color[3] = mesh->chunk_faces[iter.chunk][iter.offset].color_rgba[3];

        /* use center in z=0 plane; this provides correct results in our special setting */
        zrefpoint[0] = 0.25f * (mesh->chunk_faces[iter.chunk][iter.offset].vertices[0][0] +
                                mesh->chunk_faces[iter.chunk][iter.offset].vertices[1][0] +
                                mesh->chunk_faces[iter.chunk][iter.offset].vertices[2][0] +
                                mesh->chunk_faces[iter.chunk][iter.offset].vertices[3][0]);
        zrefpoint[1] = 0.25f * (mesh->chunk_faces[iter.chunk][iter.offset].vertices[0][1] +
                                mesh->chunk_faces[iter.chunk][iter.offset].vertices[1][1] +
                                mesh->chunk_faces[iter.chunk][iter.offset].vertices[2][1] +
                                mesh->chunk_faces[iter.chunk][iter.offset].vertices[3][1]);
        zrefpoint[2] = 0.0f;
        mesh_export_world_to_screen(projection, zrefpoint, zrefproj);

        face->zvalue = zrefproj[2];

        faces = g_list_prepend(faces, face);
    }

    if (bounding_box) {
        bounding_box->x = xr[0];
        bounding_box->y = yr[0];
        bounding_box->width = xr[1] - xr[0];
        bounding_box->height = yr[1] - yr[0];
    }

    return g_list_sort(faces, (GCompareFunc)mesh_export_sort_zlevel);
}

void mesh_export_get_bounding_box(MatrixMesh *mesh, double *projection, UtilRectangle *bounding_box)
{
    /* everything is inside a unit cube between mesh->range[0] and mesh->range[1] */
    double w[3];
    double s[3];

    double xr[2];
    double yr[2];

    w[0] = -0.5f;
    w[1] = -0.5f;
    w[2] = mesh->zrange[0];
    mesh_export_world_to_screen(projection, w, s);
    xr[0] = xr[1] = s[0];
    yr[0] = yr[1] = s[1];

#define EXPAND_BOX(wx, wy, wz) do {\
    w[0] = (wx); w[1] = (wy); w[2] = (wz);\
    mesh_export_world_to_screen(projection, w, s);\
    if (s[0] < xr[0]) xr[0] = s[0];\
    if (s[0] > xr[1]) xr[1] = s[0];\
    if (s[1] < yr[0]) yr[0] = s[1];\
    if (s[1] > yr[1]) yr[1] = s[1];\
    } while (0)

    EXPAND_BOX(-0.5f, 0.5f, mesh->zrange[0]);
    EXPAND_BOX(0.5f, 0.5f, mesh->zrange[0]);
    EXPAND_BOX(0.5f, -0.5f, mesh->zrange[0]);

    EXPAND_BOX(-0.5f, -0.5f, mesh->zrange[1]);
    EXPAND_BOX(-0.5f, 0.5f, mesh->zrange[1]);
    EXPAND_BOX(0.5f, 0.5f, mesh->zrange[1]);
    EXPAND_BOX(0.5f, -0.5f, mesh->zrange[1]);

#undef EXPAND_BOX

    if (bounding_box) {
        bounding_box->x = xr[0];
        bounding_box->y = yr[0];
        bounding_box->width = (xr[1]-xr[0]+1.0f);
        bounding_box->height = (yr[1]-yr[0]+1.0f);
    }
}

void mesh_render_faces(cairo_t *cr, GList *faces)
{
    GList *tmp;
    struct SVGFace *face;
    cairo_set_line_width(cr, 0.4f);
    for (tmp = faces; tmp; tmp = g_list_next(tmp)) {
        face = (struct SVGFace *)tmp->data;
#ifdef DEBUG
        fprintf(stderr, "face: (%.2f,%.2f,%.2f) -> (%.2f,%.2f,%.2f) -> (%.2f,%.2f,%.2f) -> (%.2f,%.2f,%.2f) [zlevel: %.2f; color: %.2f,%.2f,%.2f]\n",
                face->vertices[0][0], face->vertices[0][1], face->vertices[0][2],
                face->vertices[1][0], face->vertices[1][1], face->vertices[1][2],
                face->vertices[2][0], face->vertices[2][1], face->vertices[2][2],
                face->vertices[3][0], face->vertices[3][1], face->vertices[3][2],
                face->zvalue, face->color[0], face->color[1], face->color[2]);
#endif
        cairo_move_to(cr, face->vertices[0][0], face->vertices[0][1]);
        cairo_line_to(cr, face->vertices[1][0], face->vertices[1][1]);
        cairo_line_to(cr, face->vertices[2][0], face->vertices[2][1]);
        cairo_line_to(cr, face->vertices[3][0], face->vertices[3][1]);

        cairo_set_source_rgba(cr, face->color[0], face->color[1], face->color[2], face->color[3]);
        cairo_fill_preserve(cr);

        cairo_set_source_rgba(cr, 0.4f, 0.4f, 0.4f, 1.0f);
        cairo_stroke(cr);
    }
}

void mesh_render_grid(cairo_t *cr, UtilRectangle *bounding_box)
{
    int i;
    gchar buffer[256];
    cairo_set_source_rgba(cr, 0.3f, 0.3f, 0.3f, 0.7f);
    cairo_set_font_size(cr, 64);
    for (i = 0; i * 72.0f <= bounding_box->width; ++i) {
        cairo_move_to(cr, bounding_box->x + i * 72.0f, bounding_box->y + 0.0f);
        cairo_line_to(cr, bounding_box->x + i * 72.0f, bounding_box->y + bounding_box->height);
        cairo_stroke(cr);
        cairo_move_to(cr, bounding_box->x + i * 72.0f, bounding_box->y + 90.0f);
        sprintf(buffer, "%.02f", i * 72.0f);
        cairo_show_text(cr, buffer);
    }
    for (i = 0; i * 72.0f <= bounding_box->height; ++i) {
        cairo_move_to(cr, bounding_box->x + 0.0f, bounding_box->y + i * 72.0f);
        cairo_line_to(cr, bounding_box->x + bounding_box->width, bounding_box->y + i * 72.0f);
        cairo_stroke(cr);
        cairo_move_to(cr, bounding_box->x + 0.0f, bounding_box->y + i * 72.0f);
        sprintf(buffer, "%.02f", i * 72.0f);
        cairo_show_text(cr, buffer);
    }
}

gboolean mesh_export_to_file(const gchar *filename, ExportFileType type, MatrixMesh *mesh, double *projection)
{
    g_return_val_if_fail(mesh != NULL, FALSE);

    cairo_surface_t *surface = NULL;
    cairo_t *cr = NULL;

    UtilRectangle bounding_box;
    GList *faces = mesh_export_generate_faces(mesh, projection, &bounding_box);

    /*mesh_export_get_bounding_box(mesh, projection, &bounding_box);*/
    g_print("bounding box: @(%f, %f) %f x %f\n", bounding_box.x, bounding_box.y, bounding_box.width, bounding_box.height);

    switch (type) {
        case ExportFileTypeSVG:
            surface = cairo_svg_surface_create(filename, bounding_box.width, bounding_box.height);
            break;
        case ExportFileTypePDF:
            surface = cairo_pdf_surface_create(filename, bounding_box.width, bounding_box.height);
            break;
        case ExportFileTypePNG:
            /* TODO: image surface, get data, write to png */
            return FALSE;
        default:
            return FALSE;
    }

    cr = cairo_create(surface);
    cairo_translate(cr, -bounding_box.x, -bounding_box.y);

    mesh_render_faces(cr, faces);
/*    mesh_render_grid(cr, &bounding_box);*/

    g_list_free_full(faces, g_free);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    return TRUE;
}

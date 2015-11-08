#include "mesh-export.h"
#include "util-projection.h"
#include "util-rectangle.h"

#include <cairo.h>
#include <cairo-svg.h>
#include <cairo-pdf.h>

enum SVGFaceFlags {
    SVGFF_Required = (1 << 0)
};

struct SVGFace {
    double vertices[4][3];
    double color[4];
    UtilRectangle bounding_box;
    double zvalue;
    guint32 flags;
};

ExportFileType mesh_export_get_type_from_filename(const gchar *filename)
{
    if (g_str_has_suffix(filename, ".pdf"))
        return ExportFileTypePDF;
    if (g_str_has_suffix(filename, ".svg"))
        return ExportFileTypeSVG;
    if (g_str_has_suffix(filename, ".png"))
        return ExportFileTypePNG;
    if (g_str_has_suffix(filename, ".tex"))
        return ExportFileTypeTikZ;

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
/*#ifdef DEBUG
    fprintf(stderr, "(%.2f, %.2f, %.2f) -> (%.2f, %.2f, %.2f) [%.2f, %.2f, %.2f]\n",
            wv[0], wv[1], wv[2], sv[0], sv[1], sv[2], vs[0], vs[1], vs[2]);
#endif*/
}

gint mesh_export_sort_zlevel(struct SVGFace *a, struct SVGFace *b)
{
    if (a->zvalue < b->zvalue)
        return -1;
    if (a->zvalue > b->zvalue)
        return 1;
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

/*#ifdef DEBUG
    for (j=0; j < 16; j+=4)
        fprintf(stderr, "proj: [%.3f %.3f %.3f %.3f]\n", projection[j], projection[j+1], projection[j+2], projection[j+3]);
#endif*/

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
/*#ifdef DEBUG
        fprintf(stderr, "face: (%.2f,%.2f,%.2f) -> (%.2f,%.2f,%.2f) -> (%.2f,%.2f,%.2f) -> (%.2f,%.2f,%.2f) [zlevel: %.2f; color: %.2f,%.2f,%.2f]\n",
                face->vertices[0][0], face->vertices[0][1], face->vertices[0][2],
                face->vertices[1][0], face->vertices[1][1], face->vertices[1][2],
                face->vertices[2][0], face->vertices[2][1], face->vertices[2][2],
                face->vertices[3][0], face->vertices[3][1], face->vertices[3][2],
                face->zvalue, face->color[0], face->color[1], face->color[2]);
#endif*/
        cairo_move_to(cr, face->vertices[0][0], face->vertices[0][1]);
        cairo_line_to(cr, face->vertices[1][0], face->vertices[1][1]);
        cairo_line_to(cr, face->vertices[2][0], face->vertices[2][1]);
        cairo_line_to(cr, face->vertices[3][0], face->vertices[3][1]);
        cairo_close_path(cr);

        cairo_set_source_rgba(cr, face->color[0], face->color[1], face->color[2], face->color[3]);
        cairo_fill_preserve(cr);

        cairo_set_source_rgba(cr, 0.4f, 0.4f, 0.4f, 1.0f);
        cairo_stroke(cr);
    }
}

void _mesh_render_faces_tikz_define_color(guint32 key, double *value, FILE *file)
{
    fprintf(file, "\\definecolor{matcol%06x}{rgb}{%f,%f,%f}\n",
            key, value[0], value[1], value[2]);
}

void mesh_render_faces_tikz(FILE *file, GList *faces, UtilRectangle *bounding_box, ExportConfig *config)
{
    GList *tmp;
    struct SVGFace *face;
    guint8 j;

#define COLORHASH(col) (((guint8)(255 * (col)[0])) << 16 | ((guint8)(255 * (col)[1])) << 8 | ((guint8)(255 * (col)[2])))

    /* FIXME: make this configurable by user */
    double image_width = (config && config->image_width > 0) ? config->image_width : 15;
    double scale = image_width / bounding_box->width;

    if (config && config->standalone)
        fprintf(file, "\\documentclass{standalone}\n\\usepackage{tikz}\n\n\\begin{document}\n\\begin{tikzpicture}\n");

    fprintf(file, "\\definecolor{edgecolor}{rgb}{0.4,0.4,0.4}\n");

    /* collect colors */
    GHashTable *colortable = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
    guint32 colorhash;
    double *colorvalue;
    for (tmp = faces; tmp; tmp = g_list_next(tmp)) {
        face = (struct SVGFace *)tmp->data;
        colorhash = COLORHASH(face->color);
        colorvalue = g_malloc(4 * sizeof(double));
        colorvalue[0] = face->color[0];
        colorvalue[1] = face->color[1];
        colorvalue[2] = face->color[2];
        colorvalue[3] = face->color[3];
        g_hash_table_insert(colortable, GUINT_TO_POINTER(colorhash), colorvalue);
    }

    g_hash_table_foreach(colortable, (GHFunc)_mesh_render_faces_tikz_define_color, file);

    g_hash_table_destroy(colortable);

    for (tmp = faces; tmp; tmp = g_list_next(tmp)) {
        face = (struct SVGFace *)tmp->data;

        fprintf(file, "\t\\draw[color=edgecolor,fill=matcol%06x] ", COLORHASH(face->color));
        for (j = 0; j < 4; ++j) {
            fprintf(file, "(%f,%f) -- ",
                    (face->vertices[j][0] - bounding_box->x) * scale,
                    (bounding_box->height - face->vertices[j][1]) * scale);
        }
        fprintf(file, "cycle;\n");
    }

    if (config && config->standalone)
        fprintf(file, "\\end{tikzpicture}\n\\end{document}\n");

#undef COLORHASH
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

gboolean _mesh_point_in_face(double *vertex, struct SVGFace *face)
{
    /* face is convex; determine winding direction, i.e., take barycenter b and evaluate
     * -y * b_x + x * b_y </> 0; (x,y) = (p1_x-p0_x,p1_y-p0_y) 
     * if vertex has the same sign for all edges as this it is inside */
    double b[2] = { 0.25f * (face->vertices[0][0] + face->vertices[1][0] + face->vertices[2][0] + face->vertices[3][0]),
                    0.25f * (face->vertices[0][1] + face->vertices[1][1] + face->vertices[2][1] + face->vertices[3][1]) };
    if (  (b[1] - face->vertices[0][1]) * (face->vertices[1][0] - face->vertices[0][0])
        - (b[0] - face->vertices[0][0]) * (face->vertices[1][1] - face->vertices[0][1]) > 0) {
        if (  (vertex[1] - face->vertices[0][1]) * (face->vertices[1][0] - face->vertices[0][0])
            - (vertex[0] - face->vertices[0][0]) * (face->vertices[1][1] - face->vertices[0][1]) <= 0)
            return FALSE;
        if (  (vertex[1] - face->vertices[1][1]) * (face->vertices[2][0] - face->vertices[1][0])
            - (vertex[0] - face->vertices[1][0]) * (face->vertices[2][1] - face->vertices[1][1]) <= 0)
            return FALSE;
        if (  (vertex[1] - face->vertices[2][1]) * (face->vertices[3][0] - face->vertices[2][0])
            - (vertex[0] - face->vertices[2][0]) * (face->vertices[3][1] - face->vertices[2][1]) <= 0)
            return FALSE;
        if (  (vertex[1] - face->vertices[3][1]) * (face->vertices[0][0] - face->vertices[3][0])
            - (vertex[0] - face->vertices[3][0]) * (face->vertices[0][1] - face->vertices[3][1]) <= 0)
            return FALSE;
    }
    else {
        if (  (vertex[1] - face->vertices[0][1]) * (face->vertices[1][0] - face->vertices[0][0])
            - (vertex[0] - face->vertices[0][0]) * (face->vertices[1][1] - face->vertices[0][1]) >= 0)
            return FALSE;
        if (  (vertex[1] - face->vertices[1][1]) * (face->vertices[2][0] - face->vertices[1][0])
            - (vertex[0] - face->vertices[1][0]) * (face->vertices[2][1] - face->vertices[1][1]) >= 0)
            return FALSE;
        if (  (vertex[1] - face->vertices[2][1]) * (face->vertices[3][0] - face->vertices[2][0])
            - (vertex[0] - face->vertices[2][0]) * (face->vertices[3][1] - face->vertices[2][1]) >= 0)
            return FALSE;
        if (  (vertex[1] - face->vertices[3][1]) * (face->vertices[0][0] - face->vertices[3][0])
            - (vertex[0] - face->vertices[3][0]) * (face->vertices[0][1] - face->vertices[3][1]) >= 0)
            return FALSE;
    }
    return TRUE;
}

GList *mesh_remove_hidden_faces(GList *faces)
{
#ifdef DEBUG
    fprintf(stderr, "remove hidden faces\n");
#endif
    /* traverse faces from front to top
     * for each face:
     *   check whether one vertex is truely outside all other faces -> mark face as required
     * for each face not marked as required:
     *   for each edge:
     *     determine intersection points with preceeding faces and check intermediate points*/
    GList *visible_faces = NULL;
    GList *tmp, *vis;
    guint8 occluded;
    guint8 j;

    for (tmp = g_list_last(faces); tmp != NULL; tmp = g_list_previous(tmp)) {
        /* visible -> prepend visible faces; else free data (but not link) */
        occluded = 0;
        for (vis = visible_faces; vis != NULL && occluded != 0x0f; vis = g_list_next(vis)) {
            for (j = 0; j < 4; ++j) {
                if (_mesh_point_in_face(((struct SVGFace *)tmp->data)->vertices[j], (struct SVGFace *)vis->data))
                    occluded |= (1 << j);
            }
        }
        /* all vertices are occluded */
        if (occluded == 0x0f) {
            g_free(tmp->data);
        }
        else {
            ((struct SVGFace *)tmp->data)->flags |= SVGFF_Required;
            visible_faces = g_list_prepend(visible_faces, tmp->data);
        }
    }

    g_list_free(faces);

#ifdef DEBUG
    fprintf(stderr, "debug face\n");
    struct SVGFace test;
    test.vertices[1][0] = test.vertices[2][0] = test.vertices[2][1] = test.vertices[3][1] = 1.0;
    double vertex[3] = { 0.2, 0.1, 0.0 };
    _mesh_point_in_face(vertex, &test);
#endif

    return visible_faces;
}

gboolean mesh_export_to_file(const gchar *filename, ExportFileType type, MatrixMesh *mesh, double *projection,
                             ExportConfig *config)
{
    g_return_val_if_fail(mesh != NULL, FALSE);

    cairo_surface_t *surface = NULL;
    cairo_t *cr = NULL;
    FILE *file = NULL;

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
        case ExportFileTypeTikZ:
            break;
        default:
            return FALSE;
    }

    if (config && config->remove_hidden)
        faces = mesh_remove_hidden_faces(faces);
    
    switch (type) {
        case ExportFileTypePDF:
        case ExportFileTypeSVG:
            cr = cairo_create(surface);
            cairo_translate(cr, -bounding_box.x, -bounding_box.y);

            mesh_render_faces(cr, faces);

            cairo_destroy(cr);
            cairo_surface_destroy(surface);
            break;
        case ExportFileTypeTikZ:
            if ((file = fopen(filename, "w")) == NULL) {
                fprintf(stderr, "Could not open `%s'.\n", filename);
                g_list_free_full(faces, g_free);
                return FALSE;
            }

            mesh_render_faces_tikz(file, faces, &bounding_box, config);

            fclose(file);
            break;
        case ExportFileTypePNG:
            break;
        default:
            break;
    }

    g_list_free_full(faces, g_free);

    return TRUE;
}

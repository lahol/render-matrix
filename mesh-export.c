#include "mesh-export.h"
#include "util-projection.h"
#include "util-rectangle.h"
#include "util-colors.h"

#include <cairo.h>
#include <cairo-svg.h>
#include <cairo-pdf.h>
#include <pango/pangocairo.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

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

void mesh_render_faces(cairo_t *cr, GList *faces, double scale)
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
        cairo_move_to(cr, face->vertices[0][0] * scale, face->vertices[0][1] * scale);
        cairo_line_to(cr, face->vertices[1][0] * scale, face->vertices[1][1] * scale);
        cairo_line_to(cr, face->vertices[2][0] * scale, face->vertices[2][1] * scale);
        cairo_line_to(cr, face->vertices[3][0] * scale, face->vertices[3][1] * scale);
        cairo_close_path(cr);

        cairo_set_source_rgba(cr, face->color[0], face->color[1], face->color[2], face->color[3]);
        cairo_fill_preserve(cr);

        cairo_set_source_rgba(cr, 0.4f, 0.4f, 0.4f, 1.0f);
        cairo_stroke(cr);
    }
}

void _mesh_render_faces_tikz_define_color(guint32 key, double *value, FILE *file)
{
    fprintf(file, "\t\\definecolor{matcol%06x}{rgb}{%f,%f,%f}\n",
            key, value[0], value[1], value[2]);
}

void mesh_render_colorbar_tikz(FILE *file, UtilRectangle *colorbar, ExportConfig *config, double *range)
{
    guint8 j;
    for (j = 0; j < 7; ++j) {
        fprintf(file, "\t\\definecolor{colorbar%d}{rgb}{%f,%f,%f}\n",
                j, util_colors_basic_table[j][0], util_colors_basic_table[j][1], util_colors_basic_table[j][2]);
    }

    /* let rectangles slightly overlap to overcome rounding errors */
    for (j = 0; j < 6; ++j) {
        fprintf(file, "\t\\shade[bottom color=colorbar%d,top color=colorbar%d,draw=none] (%f,%f) rectangle (%f,%f);\n",
                j, j+1,
                colorbar->x, colorbar->y + j * (colorbar->height / 6) - 0.01,
                colorbar->x + colorbar->width, colorbar->y + (j+1) * (colorbar->height / 6) + 0.01);        
    }

    fprintf(file, "\t\\draw[color=black!40] (%f,%f) rectangle (%f,%f);\n",
            colorbar->x, colorbar->y, colorbar->x + colorbar->width, colorbar->y + colorbar->height);

    fprintf(file, "\t\\draw[color=black!40] (%f,%f) -- ++(0.5,0);\n", colorbar->x -0.1, colorbar->y);
    fprintf(file, "\t\\draw[color=black!40] (%f,%f) -- ++(0.5,0);\n", colorbar->x -0.1, colorbar->y + colorbar->height);
    if (config && config->colorbar_pos_x < 0) {
        fprintf(file, "\t\\node[anchor=west] at (%f, %f) {\\scriptsize $%.1f$};\n",
                colorbar->x + 0.4, colorbar->y, range[0]);
        fprintf(file, "\t\\node[anchor=west] at (%f, %f) {\\scriptsize $%.1f$};\n",
                colorbar->x + 0.4, colorbar->y + colorbar->height, range[1]);
    }
    else {
        fprintf(file, "\t\\node[anchor=east] at (%f, %f) {\\scriptsize $%.1f$};\n",
                colorbar->x - 0.1, colorbar->y, range[0]);
        fprintf(file, "\t\\node[anchor=east] at (%f, %f) {\\scriptsize $%.1f$};\n",
                colorbar->x - 0.1, colorbar->y + colorbar->height, range[1]);
    }
}

void mesh_render_colorbar_cairo(cairo_t *cr, UtilRectangle *colorbar, ExportConfig *config, double *range)
{
    cairo_pattern_t *gradient = cairo_pattern_create_linear(0.0, colorbar->y, 0.0, colorbar->y + colorbar->height);

    guint8 j;
    for (j = 0; j < 7; ++j) {
        cairo_pattern_add_color_stop_rgb(gradient, (6.0 - j)/6.0,
                util_colors_basic_table[j][0],
                util_colors_basic_table[j][1],
                util_colors_basic_table[j][2]);
    }

    cairo_rectangle(cr, colorbar->x, colorbar->y, colorbar->width, colorbar->height);

    cairo_set_source(cr, gradient);
    cairo_fill_preserve(cr);

    cairo_set_source_rgb(cr, 0.6f, 0.6f, 0.6f);
    cairo_stroke(cr);

    /* upper */
    cairo_move_to(cr, colorbar->x - 7.2/2.54, colorbar->y);
    cairo_line_to(cr, colorbar->x + 4*7.2/2.54, colorbar->y);
    cairo_stroke(cr);

    /* lower */
    cairo_move_to(cr, colorbar->x - 7.2/2.54, colorbar->y + colorbar->height);
    cairo_line_to(cr, colorbar->x + 4*7.2/2.54, colorbar->y + colorbar->height);
    cairo_stroke(cr);

    PangoLayout *layout;
    PangoFontDescription *desc;
    PangoRectangle extents;
    gchar buf[64];

    layout = pango_cairo_create_layout(cr);
    desc = pango_font_description_from_string("Times 5");
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);

    sprintf(buf, "%.1f", range[0]);
    pango_layout_set_markup(layout, buf, -1);
    pango_layout_get_extents(layout, NULL, &extents); 

    cairo_move_to(cr, colorbar->x + ((config && config->colorbar_pos_x < 0) ? 
                  + 36.0/2.54 
                : - 14.4/2.54 - (double)extents.width/(double)PANGO_SCALE),
            colorbar->y + colorbar->height - 0.5 * extents.height / (double)PANGO_SCALE);
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);

    sprintf(buf, "%.1f", range[1]);
    pango_layout_set_markup(layout, buf, -1);
    pango_layout_get_extents(layout, NULL, &extents); 

    cairo_move_to(cr, colorbar->x + ((config && config->colorbar_pos_x < 0) ?
                  + 36.0/2.54
                : - 14.4/2.54 - (double)extents.width/(double)PANGO_SCALE),
            colorbar->y - 0.5 * extents.height / (double)PANGO_SCALE);
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);

    cairo_pattern_destroy(gradient);
}

void mesh_set_coordinates_tikz(FILE *file, double *projection, UtilRectangle *bounding_box, ExportConfig *config, double *range)
{
    double screen[3];

    double xyplane[4][3] = {
        { -0.5, -0.5, 0.0 },
        { 0.5, -0.5, 0.0 },
        { 0.5, 0.5, 0.0 },
        { -0.5, 0.5, 0.0 }
    };

    guint8 j;

    double image_width = (config && config->image_width > 0) ? config->image_width : 15;
    double scale = image_width / bounding_box->width;

#define SETCOORD(z,l)    do {\
    for (j = 0; j < 4; ++j) {\
        xyplane[j][2] = (z);\
        mesh_export_world_to_screen(projection, xyplane[j], screen);\
        fprintf(file, "\t\\coordinate (bb%c%c%c) at (%f,%f);\n",\
                (j == 0 || j == 3) ? '0' : '1', (j == 2 || j == 3) ? '1' : '0', (l),\
                (screen[0] - bounding_box->x) * scale, (bounding_box->height - screen[1] + bounding_box->y) * scale);\
    } } while(0)

    SETCOORD(range[0], 'b');
    SETCOORD(0.0, '0');
    SETCOORD(range[1], 't');

#undef SETCOORD
}

void mesh_render_faces_tikz(FILE *file, GList *faces, UtilRectangle *bounding_box, double scale, ExportConfig *config)
{
    GList *tmp;
    struct SVGFace *face;
    guint8 j;

#define COLORHASH(col) (((guint8)(255 * (col)[0])) << 16 | ((guint8)(255 * (col)[1])) << 8 | ((guint8)(255 * (col)[2])))

    /* FIXME: make this configurable by user */
/*    double image_width = (config && config->image_width > 0) ? config->image_width : 15;
    double scale = image_width / bounding_box->width;*/

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

        fprintf(file, "\t\\draw[color=edgecolor,ultra thin,fill=matcol%06x] ", COLORHASH(face->color));
        for (j = 0; j < 4; ++j) {
            fprintf(file, "(%f,%f) -- ",
                    (face->vertices[j][0] - bounding_box->x) * scale,
                    (bounding_box->height - face->vertices[j][1] + bounding_box->y) * scale);
        }
        fprintf(file, "cycle;\n");
    }

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

gboolean _mesh_export_write_faces(const gchar *filename, ExportFileType type, MatrixMesh *mesh, GList *faces,
                                  double *projection, ExportConfig *config, UtilRectangle *bounding_box)
{
    FILE *file = NULL;

    cairo_surface_t *surface = NULL;
    cairo_t *cr = NULL;

    double image_width, scale, colorbar_correction;
    image_width = (config && config->image_width > 0) ? config->image_width : 15;

    UtilRectangle colorbar;
    colorbar.width = 0.3;


    switch (type) {
        case ExportFileTypeSVG:
            /* size in points = 1/72 in = 2.54/72 cm */
            image_width *= 72/2.54;

            colorbar.width *= 72/2.54;
            colorbar_correction = ((config) ? fabs(config->colorbar_pos_x) : 1.0) * 72/2.54 + colorbar.width + 7.2/2.54;

            scale = (image_width - colorbar_correction) / bounding_box->width;

            surface = cairo_svg_surface_create(filename, image_width, bounding_box->height * scale);
            break;
        case ExportFileTypePDF:
            /* size in points = 1/72 in = 2.54/72 cm */
            image_width *= 72/2.54;

            colorbar.width *= 72/2.54;
            colorbar_correction = ((config) ? fabs(config->colorbar_pos_x) : 1.0) * 72/2.54 + colorbar.width + 7.2/2.54;

            scale = (image_width - colorbar_correction) / bounding_box->width;

            surface = cairo_pdf_surface_create(filename, image_width, bounding_box->height * scale);
            break;
        case ExportFileTypePNG:
            /* TODO: image surface, get data, write to png */
            return FALSE;
        case ExportFileTypeTikZ:
            /* image width in cm */
            /* colorbar correction is in image dimension */
            colorbar_correction = ((config) ? fabs(config->colorbar_pos_x) : 1.0) + colorbar.width + 0.1;
            scale = (image_width - colorbar_correction) / bounding_box->width;
            break;
        default:
            return FALSE;
    }

    colorbar.x = (config) ? (config->colorbar_pos_x >= 0 ? (bounding_box->width) * scale + config->colorbar_pos_x : config->colorbar_pos_x - colorbar.width) : (bounding_box->width) * scale + 1.0;
    colorbar.height = bounding_box->height * scale * 0.8;
    colorbar.y = 0.5 * (bounding_box->height * scale - colorbar.height);

    if (config && config->remove_hidden)
        faces = mesh_remove_hidden_faces(faces);
   
    switch (type) {
        case ExportFileTypePDF:
        case ExportFileTypeSVG:
            cr = cairo_create(surface);
            if (config && config->colorbar_pos_x < 0)
                cairo_translate(cr, -bounding_box->x * scale + colorbar_correction, -bounding_box->y * scale);
            else
                cairo_translate(cr, -bounding_box->x * scale, -bounding_box->y * scale);

            colorbar.x = ((config) ? (config->colorbar_pos_x >= 0 ? bounding_box->width * scale + config->colorbar_pos_x * 72/2.54 :
                    config->colorbar_pos_x * 72/2.54 - colorbar.width) : bounding_box->width * scale + 72/2.54) +
                bounding_box->x * scale;
            colorbar.y += bounding_box->y * scale;

            fprintf(stderr, "img width: %f, colorbar.x: %f, cb width: %f\n", image_width, colorbar.x, colorbar.width);

            mesh_render_faces(cr, faces, scale);
            mesh_render_colorbar_cairo(cr, &colorbar, config, mesh->unscaled_range);

            cairo_destroy(cr);
            cairo_surface_destroy(surface);
            break;
        case ExportFileTypeTikZ:
            if ((file = fopen(filename, "w")) == NULL) {
                fprintf(stderr, "Could not open `%s'.\n", filename);
                return FALSE;
            }

            if (config && config->standalone)
                fprintf(file, "\\documentclass{standalone}\n\\usepackage{tikz}\n\n\\begin{document}\n\\begin{tikzpicture}\n");

            mesh_set_coordinates_tikz(file, projection, bounding_box, config, mesh->zrange);
            mesh_render_faces_tikz(file, faces, bounding_box, scale, config);
            mesh_render_colorbar_tikz(file, &colorbar, config, mesh->unscaled_range);
            /* FIXME: for colorbar_pos_x < 0 correct position of bounding box (colorbar_pos_x - 0.1)
             *        and take care of labels (maybe on right side?) */
            fprintf(file, "\\path[use as bounding box] (%f,0) rectangle ++(%f,%f);\n",
                    (config && config->colorbar_pos_x < 0) ? -colorbar_correction: 0,
                    bounding_box->width * scale + colorbar_correction, bounding_box->height * scale);

            if (config && config->standalone)
                fprintf(file, "\\end{tikzpicture}\n\\end{document}\n");

            fclose(file);
            break;
        case ExportFileTypePNG:
            break;
        default:
            break;
    }

    return TRUE;
}

gboolean mesh_export_to_file(const gchar *filename, ExportFileType type, MatrixMesh *mesh, double *projection,
                             ExportConfig *config)
{
    g_return_val_if_fail(mesh != NULL, FALSE);
    g_return_val_if_fail(filename != NULL, FALSE);
    g_return_val_if_fail(projection != NULL, FALSE);

    UtilRectangle bounding_box;
    GList *faces = mesh_export_generate_faces(mesh, projection, &bounding_box);

    /*mesh_export_get_bounding_box(mesh, projection, &bounding_box);*/
    g_print("bounding box: @(%f, %f) %f x %f\n", bounding_box.x, bounding_box.y, bounding_box.width, bounding_box.height);

    if (!_mesh_export_write_faces(filename, type, mesh, faces, projection, config, &bounding_box))
        g_printerr("Failed to write faces.\n");
    g_list_free_full(faces, g_free);

    return TRUE;
}

gchar *_mesh_export_generate_filename(const gchar *base, guint64 offset)
{
    if (base == NULL || base[0] == '\0')
        return NULL;

    /* only use the basename and not the directory part */
    gchar *dirsep = strrchr(base, '/');
    if (dirsep)
        ++dirsep;
    else
        dirsep = (gchar *)base;

    /* get suffix; we want only numbers in the real filename, not the extension */
    gchar *suff = strrchr(base, '.');
    if (!suff)
        suff = (gchar *)base + strlen(base);

    /* get last number */
    for ( ; suff >= dirsep; --suff)
        if (g_ascii_isdigit(*suff))
            break;
    if (suff < dirsep)
        return g_strdup(base);

    gchar *num = suff++;
    for ( ; num >= dirsep; --num)
        if (!g_ascii_isdigit(*num))
            break;
    ++num;

    gchar *format = g_strdup_printf("%%0%u" G_GUINT64_FORMAT, (guint)(suff - num));
    unsigned long long int n = strtoull(num, NULL, 10);
    GString *str = g_string_new_len(base, num - base);
    g_string_append_printf(str, format, n + offset);
    g_string_append(str, suff);
    g_free(format);

    return g_string_free(str, FALSE);
}

gboolean mesh_export_matrices_to_files(const gchar *filename_base, ExportFileType type, GList *matrices, double *projection,
                                       ExportConfig *config)
{
    g_return_val_if_fail(matrices != NULL, FALSE);
    g_return_val_if_fail(filename_base != NULL, FALSE);
    g_return_val_if_fail(projection != NULL, FALSE);

    guint offset;
    gchar *filename;
    GList *faces_list = NULL;
    GList *mesh_list = NULL;
    MatrixMesh *mesh;
    UtilRectangle bounding_box, bb;
    GList *tmpm, *tmpf;
    gboolean bb_initialized = FALSE;

    /* first pass: generate all faces and determine bounding box */
    for (tmpm = matrices; tmpm != NULL; tmpm = g_list_next(tmpm)) {
        mesh = matrix_mesh_new();
        matrix_mesh_set_alpha_channel(mesh, config->alpha_channel);
        matrix_mesh_set_matrix(mesh, (Matrix *)tmpm->data);
        mesh_list = g_list_prepend(mesh_list, mesh);

        faces_list = g_list_prepend(faces_list,
                mesh_export_generate_faces(mesh, projection, &bb));

        if (G_LIKELY(bb_initialized))
            util_rectangle_bounds(&bounding_box, &bounding_box, &bb);
        else
            bounding_box = bb;
    }
    faces_list = g_list_reverse(faces_list);
    mesh_list = g_list_reverse(mesh_list);

    g_print("bounding box: @(%f, %f) %f x %f\n", bounding_box.x, bounding_box.y, bounding_box.width, bounding_box.height);

    /* second pass: write files */
    for (tmpm = mesh_list, tmpf = faces_list, offset = 0;
            tmpm && tmpf;
            tmpm = g_list_next(tmpm), tmpf = g_list_next(tmpf), ++offset) {
        filename = _mesh_export_generate_filename(filename_base, offset);
        if (!_mesh_export_write_faces(filename, type, (MatrixMesh *)tmpm->data,
                    (GList *)tmpf->data, projection, config, &bounding_box))
            g_printerr("Failed to write faces for mesh %u.\n", offset + 1);
        g_list_free_full((GList *)tmpf->data, g_free);
        matrix_mesh_free((MatrixMesh *)tmpm->data);
    }
    
    return TRUE;
}

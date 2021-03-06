#pragma once

#include <glib.h>
#include "matrix-mesh.h"

typedef enum {
    ExportFileTypeUnknown = -1,
    ExportFileTypeSVG = 0,
    ExportFileTypePDF,
    ExportFileTypePNG,
    ExportFileTypeTikZ,
} ExportFileType;

typedef struct {
    ExportFileType type;
    gboolean remove_hidden;
    gboolean standalone;
    gboolean show_colorbar;
    gboolean grayscale;
    double image_width;
    double image_height;
    double colorbar_pos_x;
    double alpha_channel;

    gboolean permutate_entries;
    gboolean alternate_signs;
    gboolean shift_signs;
    gboolean log_scale;
    gboolean absolute_values;
    gboolean show_signum;
} ExportConfig;

ExportFileType mesh_export_get_type_from_filename(const gchar *filename);

gboolean mesh_export_to_file(const gchar *filename, ExportFileType type, MatrixMesh *mesh, double *projection,
                             ExportConfig *config);
gboolean mesh_export_matrices_to_files(const gchar *filename_base, ExportFileType type, GList *matrices, double *projection,
                                       ExportConfig *config);

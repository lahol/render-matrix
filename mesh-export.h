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
    double image_width;
} ExportConfig;

ExportFileType mesh_export_get_type_from_filename(const gchar *filename);

gboolean mesh_export_to_file(const gchar *filename, ExportFileType type, MatrixMesh *mesh, double *projection,
                             ExportConfig *config);

#pragma once

#include <glib.h>
#include "matrix-mesh.h"

typedef enum {
    ExportFileTypeSVG,
    ExportFileTypePDF
} ExportFileType;

gboolean mesh_export_to_file(const gchar *filename, ExportFileType type, MatrixMesh *mesh, double *projection);

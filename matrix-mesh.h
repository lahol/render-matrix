#pragma once

#include <glib.h>
#include "matrix.h"

typedef enum {
    MatrixMeshFacePlaneNone = 0,
    MatrixMeshFacePlaneXY,
    MatrixMeshFacePlaneXZ,
    MatrixMeshFacePlaneYZ
} MatrixMeshFacePlane;

typedef struct {
    double vertices[4][3];
    double color_rgb[3];
    double color_hue;

    MatrixMeshFacePlane plane;
} MatrixMeshFace;

#define MATRIX_MESH_FACE_CHUNK_SIZE 4096

typedef struct {
    guint32 chunk;
    guint32 offset;
} MatrixMeshIter;

typedef struct {
    guint64 nfaces;
    Matrix *matrix;
    MatrixMeshIter last;
    guint32 n_chunks;
    MatrixMeshFace **chunk_faces;
} MatrixMesh;

MatrixMesh *matrix_mesh_new(void);
void matrix_mesh_set_matrix(MatrixMesh *mesh, Matrix *matrix);
void matrix_mesh_update(MatrixMesh *mesh);
void matrix_mesh_iter_init(MatrixMesh *mesh, MatrixMeshIter *iter);
void matrix_mesh_free(MatrixMesh *mesh);
gboolean matrix_mesh_iter_next(MatrixMesh *mesh, MatrixMeshIter *iter);
gboolean matrix_mesh_iter_is_valid(MatrixMesh *mesh, MatrixMeshIter *iter);
MatrixMeshFace *matrix_mesh_append_face(MatrixMesh *mesh, MatrixMeshIter *iter);


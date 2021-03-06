#include "matrix-mesh.h"
#include <string.h>
#include <math.h>
#include "util-colors.h"

double z_epsilon = -1.0f;

void matrix_mesh_set_z_epsilon(double eps)
{
    z_epsilon = eps;
}

MatrixMesh *matrix_mesh_new(void)
{
    MatrixMesh *mesh = g_malloc0(sizeof(MatrixMesh));
    matrix_mesh_iter_init(mesh, &mesh->last);

    return mesh;
}

void matrix_mesh_clear(MatrixMesh *mesh)
{
    if (!mesh)
        return;
    guint32 i;
    for (i = 0; i < mesh->n_chunks; ++i)
        g_free(mesh->chunk_faces[i]);
    g_free(mesh->chunk_faces);

    double alpha_channel = mesh->alpha_channel;

    memset(mesh, 0, sizeof(MatrixMesh));

    mesh->alpha_channel = alpha_channel;
}

void matrix_mesh_free(MatrixMesh *mesh)
{
    matrix_mesh_clear(mesh);
    g_free(mesh);
}

void matrix_mesh_set_matrix(MatrixMesh *mesh, Matrix *matrix)
{
    mesh->matrix = matrix;

    matrix_mesh_update(mesh);
}

gboolean matrix_mesh_plane_face(MatrixMeshFace *face, double x, double y, double z, double d1, double d2, double alpha_channel)
{
    util_colors_gradient_rgb(face->color_hue, face->color_rgba);
    face->color_rgba[3] = alpha_channel;

    switch (face->plane) {
        case MatrixMeshFacePlaneXY:
            if (fabs(z) <= z_epsilon)
                return FALSE;
            face->vertices[0][0] = face->vertices[3][0] = x;
            face->vertices[1][0] = face->vertices[2][0] = x + d1;

            face->vertices[0][1] = face->vertices[1][1] = y;
            face->vertices[2][1] = face->vertices[3][1] = y + d2;

            face->vertices[0][2] = face->vertices[1][2] =
            face->vertices[2][2] = face->vertices[3][2] = z;
            break;
        case MatrixMeshFacePlaneXZ:
            if (fabs(z+d2) <= z_epsilon && fabs(z) <= z_epsilon)
                return FALSE;
            face->vertices[0][0] = face->vertices[3][0] = x;
            face->vertices[1][0] = face->vertices[2][0] = x + d1;

            face->vertices[0][1] = face->vertices[1][1] =
            face->vertices[2][1] = face->vertices[3][1] = y;

            face->vertices[0][2] = face->vertices[1][2] = z;
            face->vertices[2][2] = face->vertices[3][2] = z + d2;
            break;
        case MatrixMeshFacePlaneYZ:
            if (fabs(z+d2) <= z_epsilon && fabs(z) <= z_epsilon)
                return FALSE;
            face->vertices[0][0] = face->vertices[1][0] =
            face->vertices[2][0] = face->vertices[3][0] = x;

            face->vertices[1][1] = face->vertices[2][1] = y;
            face->vertices[0][1] = face->vertices[3][1] = y + d1;

            face->vertices[0][2] = face->vertices[1][2] = z;
            face->vertices[2][2] = face->vertices[3][2] = z + d2;
            break;
        default:
            return FALSE;
            fprintf(stderr, "plane orientation not handled\n");
    }

    return TRUE;
}

void matrix_mesh_set_alpha_channel(MatrixMesh *mesh, double alpha_channel)
{
    if (!mesh)
        return;
    mesh->alpha_channel = alpha_channel;

    MatrixMeshIter fiter;

    for (matrix_mesh_iter_init(mesh, &fiter);
         matrix_mesh_iter_is_valid(mesh, &fiter);
         matrix_mesh_iter_next(mesh, &fiter)) {
        mesh->chunk_faces[fiter.chunk][fiter.offset].color_rgba[3] = alpha_channel;
    }
}

void matrix_mesh_update(MatrixMesh *mesh)
{
    if (!mesh)
        return;
    Matrix *m = mesh->matrix;
    matrix_mesh_clear(mesh);
    mesh->matrix = m;

    MatrixIter miter;
    MatrixMeshIter fiter;

    MatrixMeshFace *face;

    if (!m)
        return;

    double range[2];
    double scale;

    matrix_iter_init(m, &miter);
    if (matrix_iter_is_valid(m, &miter)) {
        range[0] = range[1] = m->chunks[miter.chunk][miter.offset];
    }
    matrix_iter_next(m, &miter);
    for ( ; matrix_iter_is_valid(m, &miter); matrix_iter_next(m, &miter)) {
        if (range[0] > m->chunks[miter.chunk][miter.offset])
            range[0] = m->chunks[miter.chunk][miter.offset];
        if (range[1] < m->chunks[miter.chunk][miter.offset])
            range[1] = m->chunks[miter.chunk][miter.offset];
    }
    scale = range[0] != range[1] ? 1.0f/(range[1]-range[0]) : 1.0f;

    mesh->unscaled_range[0] = range[0];
    mesh->unscaled_range[1] = range[1];

    range[0] *= scale;
    range[1] *= scale;

    mesh->zrange[0] = range[0];
    mesh->zrange[1] = range[1];

    double dx = 1.0f/m->n_columns;
    double dy = 1.0f/m->n_rows;
    double x,y,z;

    /* FIXME: clean up this messy code (especially the face generation) */

    /* top faces */
    for (matrix_iter_init(m, &miter);
         matrix_iter_is_valid(m, &miter);
         matrix_iter_next(m, &miter)) {
        z = m->chunks[miter.chunk][miter.offset] * scale;
        x = miter.column * dx - 0.5f;
        y = 0.5f - miter.row * dy - dy;

        face = matrix_mesh_append_face(mesh, &fiter);

        face->plane = MatrixMeshFacePlaneXY;
        face->color_hue = z - range[0];

        if (!matrix_mesh_plane_face(face, x, y, z, dx, dy, mesh->alpha_channel))
            matrix_mesh_remove_last_face(mesh);
    }

    /* only render visible areas, switch colors if signs of neighbours differ otherwise
     * take color of larger absolute value */

    /* faces in yz-plane */
    guint32 i, j;
    double zl, zc;

    for (i = 0; i < m->n_rows; ++i) {
        y = 0.5f - i * dy - dy;
        for (j = 0; j < m->n_columns; ++j) {
            if (!matrix_get_iter(m, &miter, i, j))
                break;
            x = j * dx - 0.5f;

            zc = m->chunks[miter.chunk][miter.offset] * scale;
            /* first or sign change */
            if (j == 0 || zc * zl < 0) {
                face = matrix_mesh_append_face(mesh, &fiter);
                face->plane = MatrixMeshFacePlaneYZ;
                face->color_hue = zc - range[0];

                if (!matrix_mesh_plane_face(face, x, y, 0, dy, zc, mesh->alpha_channel))
                    matrix_mesh_remove_last_face(mesh);

                if (j > 0) {
                    face = matrix_mesh_append_face(mesh, &fiter);
                    face->plane = MatrixMeshFacePlaneYZ;
                    face->color_hue = zl - range[0];

                    if (!matrix_mesh_plane_face(face, x, y, 0, dy, zl, mesh->alpha_channel))
                        matrix_mesh_remove_last_face(mesh);
                }
            }
            else {
                face = matrix_mesh_append_face(mesh, &fiter);
                face->plane = MatrixMeshFacePlaneYZ;
                if (fabs(zc) > fabs(zl))
                    face->color_hue = zc - range[0];
                else
                    face->color_hue = zl - range[0];

                if (!matrix_mesh_plane_face(face, x, y, zl, dy, zc - zl, mesh->alpha_channel))
                    matrix_mesh_remove_last_face(mesh);
            }

            zl = zc;
        }

        face = matrix_mesh_append_face(mesh, &fiter);
        face->plane = MatrixMeshFacePlaneYZ;
        face->color_hue = zl - range[0];

        if (!matrix_mesh_plane_face(face, 0.5f, y, 0, dy, zl, mesh->alpha_channel))
            matrix_mesh_remove_last_face(mesh);
    }

    for (j = 0; j < m->n_columns; ++j) {
        x = j * dx - 0.5f;
        for (i = 0; i < m->n_rows; ++i) {
            if (!matrix_get_iter(m, &miter, i, j))
                break;
            y = 0.5f - i * dy;

            zc = m->chunks[miter.chunk][miter.offset] * scale;
            if (i == 0 || zc * zl < 0) {
                face = matrix_mesh_append_face(mesh, &fiter);
                face->plane = MatrixMeshFacePlaneXZ;
                face->color_hue = zc - range[0];

                if (!matrix_mesh_plane_face(face, x, y, 0, dx, zc, mesh->alpha_channel))
                    matrix_mesh_remove_last_face(mesh);

                if (i > 0) {
                    face = matrix_mesh_append_face(mesh, &fiter);
                    face->plane = MatrixMeshFacePlaneXZ;
                    face->color_hue = zl - range[0];

                    if (!matrix_mesh_plane_face(face, x, y, 0, dx, zl, mesh->alpha_channel))
                        matrix_mesh_remove_last_face(mesh);
                }
            }
            else {
                face = matrix_mesh_append_face(mesh, &fiter);
                face->plane = MatrixMeshFacePlaneXZ;
                if (fabs(zc) > fabs(zl))
                    face->color_hue = zc - range[0];
                else
                    face->color_hue = zl - range[0];

                if (!matrix_mesh_plane_face(face, x, y, zl, dx, zc - zl, mesh->alpha_channel))
                    matrix_mesh_remove_last_face(mesh);
            }

            zl = zc;
        }

        face = matrix_mesh_append_face(mesh, &fiter);
        face->plane = MatrixMeshFacePlaneXZ;
        face->color_hue = zl - range[0];

        if (!matrix_mesh_plane_face(face, x, -0.5f, 0, dx, zl, mesh->alpha_channel))
            matrix_mesh_remove_last_face(mesh);
    }
}

void matrix_mesh_iter_init(MatrixMesh *mesh, MatrixMeshIter *iter)
{
    memset(iter, 0, sizeof(MatrixMeshIter));
}

gboolean matrix_mesh_iter_next(MatrixMesh *mesh, MatrixMeshIter *iter)
{
    ++iter->offset;
    if (iter->offset == MATRIX_MESH_FACE_CHUNK_SIZE) {
        ++iter->chunk;
        iter->offset = 0;
    }

    return matrix_mesh_iter_is_valid(mesh, iter);
}

gboolean matrix_mesh_iter_is_valid(MatrixMesh *mesh, MatrixMeshIter *iter)
{
    if (iter->chunk >= mesh->n_chunks || mesh->n_chunks == 0)
        return FALSE;
    if (iter->chunk * MATRIX_MESH_FACE_CHUNK_SIZE + iter->offset >= mesh->nfaces)
        return FALSE;

    return TRUE;
}

MatrixMeshFace *matrix_mesh_append_face(MatrixMesh *mesh, MatrixMeshIter *iter)
{
    if (mesh->last.chunk == mesh->n_chunks) {
        ++mesh->n_chunks;
        mesh->chunk_faces = g_realloc(mesh->chunk_faces, mesh->n_chunks * sizeof(MatrixMeshFace *));
        mesh->chunk_faces[mesh->last.chunk] = g_malloc(MATRIX_MESH_FACE_CHUNK_SIZE * sizeof(MatrixMeshFace));
    }

    if (iter)
        *iter = mesh->last;

    MatrixMeshFace *face = &mesh->chunk_faces[mesh->last.chunk][mesh->last.offset];

    ++mesh->last.offset;
    if (mesh->last.offset == MATRIX_MESH_FACE_CHUNK_SIZE) {
        mesh->last.offset = 0;
        ++mesh->last.chunk;
    }

    ++mesh->nfaces;

    return face;
}

void matrix_mesh_remove_last_face(MatrixMesh *mesh)
{
    if (mesh->nfaces == 0)
        return;

    if (mesh->last.offset == 0) {
        --mesh->last.chunk;
        mesh->last.offset = MATRIX_MESH_FACE_CHUNK_SIZE;
    }
    else {
        --mesh->last.offset;
    }

    --mesh->nfaces;
}

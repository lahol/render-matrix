#include "util-projection.h"
#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <stdio.h>

void util_matrix_identify(double *matrix)
{
    static double id[16] = { 1.0f, 0.0f, 0.0f, 0.0f,
                             0.0f, 1.0f, 0.0f, 0.0f,
                             0.0f, 0.0f, 1.0f, 0.0f,
                             0.0f, 0.0f, 0.0f, 1.0f
                           };
    memcpy(matrix, id, sizeof(double)*16);
}

void util_rotate_matrix(double *matrix, double angle, int axis, double *coords)
{
    double rm[16];
    util_get_rotation_matrix(rm, angle, axis, coords);
    util_matrix_multiply(rm, matrix, matrix);
}

void util_scale_matrix(double *matrix, double *scale_v)
{
    int i, i4;
    for (i = 0; i < 4; i++) {
        i4 = 4*i;
        matrix[i4] *= scale_v[0];
        matrix[i4+1] *= scale_v[1];
        matrix[i4+2] *= scale_v[2];
    }
}

void util_translate_matrix(double *matrix, double *translate_v)
{
    int i, i4;
    for (i = 0; i < 4; i++) {
        i4 = 4*i;
        matrix[i4  ] += matrix[i4+3]*translate_v[0];
        matrix[i4+1] += matrix[i4+3]*translate_v[1];
        matrix[i4+2] += matrix[i4+3]*translate_v[2];
    }
}

/* transposes of the rotation matrices due to the fact that opengl has some weird
 * rules for multiplication, i.e. using the transposes */
void util_get_rotation_matrix(double *matrix, double angle, int axis, double *coords)
{
    double sin_x, cos_x;
    double rad = angle/180.0f*M_PI;
    double n[3], len;
    sin_x = sin(rad);
    cos_x = cos(rad);
    util_matrix_identify(matrix);
    switch (axis) {
        case UTIL_AXIS_X:
            matrix[5] = cos_x;
            matrix[6] = sin_x;
            matrix[9] = -sin_x;
            matrix[10] = cos_x;
            break;
        case UTIL_AXIS_Y:
            matrix[0] = cos_x;
            matrix[2] = -sin_x;
            matrix[8] = sin_x;
            matrix[10] = cos_x;
            break;
        case UTIL_AXIS_Z:
            matrix[0] = cos_x;
            matrix[1] = sin_x;
            matrix[4] = -sin_x;
            matrix[5] = cos_x;
            break;
        case UTIL_AXIS_CUSTOM:
            if (!coords)
               break;
            len = sqrt(coords[0]*coords[0]+coords[1]*coords[1]+coords[2]*coords[2]);
            n[0] = coords[0] / len;
            n[1] = coords[1] / len;
            n[2] = coords[2] / len;

            matrix[0]  = n[0] * n[0] * (1 - cos_x) + cos_x;
            matrix[4]  = n[0] * n[1] * (1 - cos_x) - n[2] * sin_x;
            matrix[8]  = n[0] * n[2] * (1 - cos_x) + n[1] * sin_x;

            matrix[1]  = n[1] * n[0] * (1 - cos_x) + n[2] * sin_x;
            matrix[5]  = n[1] * n[1] * (1 - cos_x) + cos_x;
            matrix[9]  = n[1] * n[2] * (1 - cos_x) - n[0] * sin_x;

            matrix[2]  = n[2] * n[0] * (1 - cos_x) - n[1] * sin_x;
            matrix[6]  = n[2] * n[1] * (1 - cos_x) + n[0] * sin_x;
            matrix[10] = n[2] * n[2] * (1 - cos_x) + cos_x;
            break;
    }
}

/* compute azimuth, elevation, tilt from (transposed) rotation matrix */
/* transformation is expected in the form
 *  1. azimuth around z-axis
 *  2. elevation around x-axis 
 *  3. tilt */
void util_rotation_matrix_get_eulerian_angels(double *matrix, double *angles)
{
#if 0
    /* get angle and axis */
    double theta = acos(0.5f * (matrix[0] + matrix[5] + matrix[10] - 1.0f));
    double sin_t = 0.5f/sin(theta);
    double n[3] = {
        (matrix[6] - matrix[9]) * sin_t,
        (matrix[8] - matrix[2]) * sin_t,
        (matrix[1] - matrix[4]) * sin_t
    };

    /* n is either of length 1 or 0 (i.e. identity matrix) */
    if (n[0]*n[0] + n[1]*n[1] + n[2]*n[2] < 0.001f) {
        angles[0] = angles[1] = angles[2] = 0.0f;
        return;
    }
#endif
    double azimuth = 0, elevation = 0, tilt = 0;

    /* multiply rotation matrices and compare entries; take care of sign of cos(elevation) = a_33 */
    /* unique up to sign of acos(matrix[10]) */
    elevation = M_PI - acos(-matrix[10]);
    double sin_e = sqrt(1.0f - matrix[10] * matrix[10]);
    fprintf(stderr, "sin_e: %f\n", sin_e);
    if (sin_e < 0.0001f) {
        /* no real elavation (azimuth + tilt) not uniquely defined*/
        azimuth = acos(matrix[0]);
        if (matrix[1] < 0)
            azimuth = -azimuth;
        tilt = 0.0f;
    }
    else {
        tilt = M_PI - acos(-matrix[9]/sin_e);
        if (matrix[8] > 0)
            tilt = -tilt;
        if (tilt < 0.0001f && tilt > -0.0001f)
            tilt = 0.0f;
        azimuth = M_PI - acos(matrix[6]/sin_e);
        if (matrix[2] > 0)
            azimuth = 2*M_PI - azimuth;

        if ((tilt >= 0 && matrix[8] <= 0) || (tilt <= 0 && matrix[8] >= 0))
            elevation = -elevation;
    }

    angles[0] = azimuth * 180.0f * M_1_PI;
    angles[1] = elevation * 180.0f * M_1_PI;
    angles[2] = tilt * 180.0f * M_1_PI;
}

void util_matrix_multiply(double *A, double *B, double *target)
{
    int i, im4, id4;
    double m[16];
    double *t;
    if (A == target || B == target)
        t = m;
    else
        t = target;
    for (i = 0; i < 16; i++) {
        im4 = i%4;
        id4 = i-im4;
        t[i] = A[im4]  * B[id4]   + A[im4+4]  * B[id4+1] +
               A[im4+8]* B[id4+2] + A[im4+12] * B[id4+3];
    }
    if (A == target || B == target)
        memcpy(target, m, sizeof(double) * 16);
}

void util_transpose_matrix(double *matrix)
{
    int i, j;
    double t;
    for (i = 0; i < 4; i++) {
        for (j = i+1; j < 4; j++) {
            if (i != j) {
                t = matrix[4*i+j];
                matrix[4*i+j] = matrix[4*j+i];
                matrix[4*j+i] = t;
            }
        }
    }
}

void util_vector_matrix_multiply(double *vect, double *matrix, double *target)
{
    target[0] = vect[0]*matrix[0]+vect[1]*matrix[4]+vect[2]*matrix[8]+vect[3]*matrix[12];
    target[1] = vect[0]*matrix[1]+vect[1]*matrix[5]+vect[2]*matrix[9]+vect[3]*matrix[13];
    target[2] = vect[0]*matrix[2]+vect[1]*matrix[6]+vect[2]*matrix[10]+vect[3]*matrix[14];
    target[3] = vect[0]*matrix[3]+vect[1]*matrix[7]+vect[2]*matrix[11]+vect[3]*matrix[15];
}

void util_matrix_vector_multiply(double *matrix, double *vect, double *target)
{
    target[0] = vect[0]*matrix[0]+vect[1]*matrix[1]+vect[2]*matrix[2]+vect[3]*matrix[3];
    target[1] = vect[0]*matrix[4]+vect[1]*matrix[5]+vect[2]*matrix[6]+vect[3]*matrix[7];
    target[2] = vect[0]*matrix[8]+vect[1]*matrix[9]+vect[2]*matrix[10]+vect[3]*matrix[11];
    target[3] = vect[0]*matrix[12]+vect[1]*matrix[13]+vect[2]*matrix[14]+vect[3]*matrix[15];
}

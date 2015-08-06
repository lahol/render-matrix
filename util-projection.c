#include "util-projection.h"
#include <stdlib.h>
#include <memory.h>
#include <math.h>

void util_matrix_identify(double *matrix) {
  static double id[16] = { 1.0f, 0.0f, 0.0f, 0.0f,
                           0.0f, 1.0f, 0.0f, 0.0f,
                           0.0f, 0.0f, 1.0f, 0.0f,
                           0.0f, 0.0f, 0.0f, 1.0f };
  memcpy(matrix, id, sizeof(double)*16);
}

void util_rotate_matrix(double *matrix, double angle, int axis) {
  double rm[16], m[16];
  memcpy(m, matrix, sizeof(double)*16);
  util_get_rotation_matrix(rm, angle, axis);
  util_matrix_multiply(rm, m, matrix);
}

void util_scale_matrix(double *matrix, double *scale_v) {
  int i, i4;
  for (i = 0; i < 4; i++) {
    i4 = 4*i;
    matrix[i4] *= scale_v[0];
    matrix[i4+1] *= scale_v[1];
    matrix[i4+2] *= scale_v[2];
  }
}

void util_translate_matrix(double *matrix, double *translate_v) {
  int i, i4;
  for (i = 0; i < 4; i++) {
    i4 = 4*i;
    matrix[i4  ] += matrix[i4+3]*translate_v[0];
    matrix[i4+1] += matrix[i4+3]*translate_v[1];
    matrix[i4+2] += matrix[i4+3]*translate_v[2];
  }
}

void util_get_rotation_matrix(double *matrix, double angle, int axis) {
  double sin_x, cos_x;
  double rad = angle/180.0f*3.1415926535f;
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
  }
}

void util_matrix_multiply(double *A, double *B, double *target) {
  int i, im4, id4;
  for (i = 0; i < 16; i++) {
    im4 = i%4;
    id4 = i-im4;
    target[i] = A[im4]  * B[id4]   + A[im4+4]  * B[id4+1] +
                A[im4+8]* B[id4+2] + A[im4+12] * B[id4+3];
  }
}

void util_transpose_matrix(double *matrix) {
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

void util_vector_matrix_multiply(double *vect, double *matrix, double *target) {
  target[0] = vect[0]*matrix[0]+vect[1]*matrix[4]+vect[2]*matrix[8]+vect[3]*matrix[12];
  target[1] = vect[0]*matrix[1]+vect[1]*matrix[5]+vect[2]*matrix[9]+vect[3]*matrix[13];
  target[2] = vect[0]*matrix[2]+vect[1]*matrix[6]+vect[2]*matrix[10]+vect[3]*matrix[14];
  target[3] = vect[0]*matrix[3]+vect[1]*matrix[7]+vect[2]*matrix[11]+vect[3]*matrix[15];
}

void util_matrix_vector_multiply(double *matrix, double *vect, double *target) {
  target[0] = vect[0]*matrix[0]+vect[1]*matrix[1]+vect[2]*matrix[2]+vect[3]*matrix[3];
  target[1] = vect[0]*matrix[4]+vect[1]*matrix[5]+vect[2]*matrix[6]+vect[3]*matrix[7];
  target[2] = vect[0]*matrix[8]+vect[1]*matrix[9]+vect[2]*matrix[10]+vect[3]*matrix[11];
  target[3] = vect[0]*matrix[12]+vect[1]*matrix[13]+vect[2]*matrix[14]+vect[3]*matrix[15];
}

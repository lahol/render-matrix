#ifndef __PROJECTION_H__
#define __PROJECTION_H__

#define UTIL_AXIS_X      0
#define UTIL_AXIS_Y      1
#define UTIL_AXIS_Z      2
#define UTIL_AXIS_CUSTOM 3

void util_matrix_identify(double *matrix);
void util_rotate_matrix(double *matrix, double angle, int axis, double *coords);
void util_scale_matrix(double *matrix, double *scale_v);
void util_translate_matrix(double *matrix, double *translate_v);
void util_get_rotation_matrix(double *matrix, double angle, int axis, double *coords);
void util_rotation_matrix_get_eulerian_angels(double *matrix, double *angles);
void util_matrix_multiply(double *A, double *B, double *target);
void util_transpose_matrix(double *matrix);
void util_vector_matrix_multiply(double *vect, double *matrix, double *target);
void util_matrix_vector_multiply(double *matrix, double *vect, double *target);

#endif

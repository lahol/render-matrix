#pragma once

#include <glib.h>
#include <stdio.h>

#define MATRIX_CHUNK_SIZE 4096

typedef struct {
    guint32 row;
    guint32 column;
    guint32 chunk;
    guint32 offset;
} MatrixIter;

typedef struct {
    guint32 n_rows;
    guint32 n_columns;
    guint32 n_chunks;
    MatrixIter last;
    double **chunks;
} Matrix;

Matrix *matrix_new(void);
void matrix_iter_init(Matrix *matrix, MatrixIter *iter);
void matrix_free(Matrix *matrix);
gboolean matrix_iter_next(Matrix *matrix, MatrixIter *iter);
gboolean matrix_iter_is_valid(Matrix *matrix, MatrixIter *iter);
/*void matrix_set_value(Matrix *matrix, MatrixIter *iter, double value);*/
void matrix_append_value(Matrix *matrix, MatrixIter *iter, double value);
gboolean matrix_get_iter(Matrix *matrix, MatrixIter *iter, guint32 row, guint32 column);

GList *matrix_read_from_file(int fd);

void matrix_copy(Matrix *dst, Matrix *src);
Matrix *matrix_dup(Matrix *matrix);
void matrix_permutate_matrix(Matrix *matrix);
void matrix_alternate_signs(Matrix *matrix, gboolean do_shift);
void matrix_log_scale(Matrix *matrix);
void matrix_set_absolute(Matrix *matrix);
void matrix_set_signum(Matrix *matrix);

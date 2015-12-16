#include "matrix.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

Matrix *matrix_new(void)
{
    Matrix *m = g_malloc0(sizeof(Matrix));
    matrix_iter_init(m, &m->last);

    return m;
}

void matrix_clear(Matrix *matrix)
{
    if (!matrix)
        return;
    guint32 i;
    for (i = 0; i < matrix->n_chunks; ++i)
        g_free(matrix->chunks[i]);
    g_free(matrix->chunks);

    memset(matrix, 0, sizeof(Matrix));
}

void matrix_free(Matrix *matrix)
{
    matrix_clear(matrix);
    g_free(matrix);
}

void matrix_iter_init(Matrix *matrix, MatrixIter *iter)
{
    memset(iter, 0, sizeof(MatrixIter));
}

gboolean matrix_get_iter(Matrix *matrix, MatrixIter *iter, guint32 row, guint32 column)
{
    iter->row = row;
    iter->column = column;
    iter->chunk = (row * matrix->n_columns + column)  / MATRIX_CHUNK_SIZE;
    iter->offset = (row * matrix->n_columns + column) % MATRIX_CHUNK_SIZE;

    return matrix_iter_is_valid(matrix, iter);
}

gboolean matrix_iter_next(Matrix *matrix, MatrixIter *iter)
{
    ++iter->column;
    if (iter->column == matrix->n_columns) {
        ++iter->row;
        iter->column = 0;
    }

    return matrix_get_iter(matrix, iter, iter->row, iter->column);
}

gboolean matrix_iter_is_valid(Matrix *matrix, MatrixIter *iter)
{
    if (iter->column >= matrix->n_columns || iter->row >= matrix->n_rows)
        return FALSE;
    /* Check for offset/chunk? */
    return TRUE;
}

void matrix_append_value(Matrix *matrix, MatrixIter *iter, double value)
{
    if (matrix->last.chunk == matrix->n_chunks) {
        ++matrix->n_chunks;
        matrix->chunks = g_realloc(matrix->chunks, matrix->n_chunks * sizeof(double *));
        matrix->chunks[matrix->last.chunk] = g_malloc(MATRIX_CHUNK_SIZE * sizeof(double));
    }

    matrix->chunks[matrix->last.chunk][matrix->last.offset] = value;
    if (iter)
        *iter = matrix->last;

    ++matrix->last.offset;
    if (matrix->last.offset == MATRIX_CHUNK_SIZE) {
        matrix->last.offset = 0;
        ++matrix->last.chunk;
    }
}

void matrix_set_value(Matrix *matrix, MatrixIter *iter, double value)
{
}

Matrix *test_matrix(void)
{
    Matrix *m = matrix_new();
    guint32 i;
    for (i = 0; i < 100; ++i) {
        matrix_append_value(m, NULL, drand48());
    }
    m->n_columns = 10;
    m->n_rows = 10;

    return m;
}

/*typedef enum {
    TOKEN_NUMBER,
    TOKEN_NEWLINE
} MatrixTokenType;

gboolean _matrix_read_next_token(FILE *infile, MatrixTokenType *type, double *value)
{
    if (feof(infile))
        return FALSE;

    int c;
    gchar buffer[256];
    guint i;
    do {
        c = fgetc(infile);
    } while (c >= 0 && c != '\n' && g_ascii_isspace(c));

    if (c < 0)
        return FALSE;
    if (c == '\n') {
        if (type) *type = TOKEN_NEWLINE;
        return TRUE;
    }

    while (c == '-' || c == '.' || g_ascii_isdigit(c)) {
        buffer[i++] = c;
        c = fgetc(c);
    }
    buffer[i] = 0;

}*/

GList *matrix_read_from_file(int fd)
{
    GScanner *scanner = g_scanner_new(NULL);
    scanner->config->cset_skip_characters = " \t";
    scanner->config->int_2_float = 1;
    g_scanner_input_file(scanner, fd);

    GTokenType next_token_type;
    gboolean negate = FALSE;

    Matrix *m;
    guint32 columns = 0;

    GList *list = NULL;

    m = matrix_new(); 
    list = g_list_prepend(list, m);

    do {
        next_token_type = g_scanner_get_next_token(scanner);
        g_scanner_peek_next_token(scanner);

        /* TODO do not allow 0.5-1.2 as two floats */

        if (next_token_type == G_TOKEN_FLOAT) {
            ++columns;
            matrix_append_value(m, NULL, negate ? -scanner->value.v_float : scanner->value.v_float);
            negate = FALSE;
        }
        else if (next_token_type == '-') {
            negate = TRUE;
        }
        else if (next_token_type == '\n') {
            if (m->n_columns != 0 && m->n_columns != columns) {
                if (columns == 0) {
                    g_print("start of new matrix at line %u\n", scanner->line);
                    m = matrix_new();
                    list = g_list_prepend(list, m);
                    continue;
                }
                else {
                    g_printerr("column mismatch at line %u\n", scanner->line);
                }
            }
            else if (m->n_columns == 0) {
                    m->n_columns = columns;
            }
            ++m->n_rows;
            columns = 0;
        }
        else {
            g_print("type: %d\n", next_token_type);
        }
    } while (scanner->next_token != G_TOKEN_EOF &&
             scanner->next_token != G_TOKEN_ERROR);

    g_scanner_destroy(scanner);

    GList *llink = list;
    if (llink && llink->data && ((Matrix *)llink->data)->n_columns == 0) {
        list = g_list_remove_link(list, llink);
        g_list_free_full(llink, (GDestroyNotify)matrix_free);
    }

    return g_list_reverse(list);
}

void matrix_copy(Matrix *dst, Matrix *src)
{
    if (dst == src || !dst || !src)
        return;
    matrix_clear(dst);
    *dst = *src;

    dst->chunks = g_malloc0(dst->n_chunks * sizeof(double *));
    guint32 i;
    for (i = 0; i < dst->n_chunks; ++i) {
        dst->chunks[i] = g_malloc(MATRIX_CHUNK_SIZE * sizeof(double));
        memcpy(dst->chunks[i], src->chunks[i], MATRIX_CHUNK_SIZE * sizeof(double));
    }

}

Matrix *matrix_dup(Matrix *matrix)
{
    if (!matrix)
        return NULL;

    Matrix *dup = matrix_new();
    matrix_copy(dup, matrix);

    return dup;
}

/* Permutate the matrix so that we have the blocks
 * [ c_{2i,2j}   | c_{2i,2j+1}   ]
 * [-----------------------------]
 * [ c_{2i+1,2j} | c_{2i+1,2j+1} ]*/
void matrix_permutate_matrix(Matrix *matrix)
{
    Matrix *tmp = matrix_dup(matrix);
    guint32 i, j;
    MatrixIter iter1;
    MatrixIter iter2;

    guint32 oi = (matrix->n_rows + 1)/2;
    guint32 oj = (matrix->n_columns + 1)/2;

    for (i = 0; i < matrix->n_rows; i += 2) {
        for (j = 0; j < matrix->n_columns; j += 2) {
            matrix_get_iter(tmp, &iter1, i, j);
            matrix_get_iter(matrix, &iter2, i/2, j/2);
            matrix->chunks[iter2.chunk][iter2.offset] = tmp->chunks[iter1.chunk][iter1.offset];
        }
    }
    for (i = 0; i < matrix->n_rows; i += 2) {
        for (j = 1; j < matrix->n_columns; j += 2) {
            matrix_get_iter(tmp, &iter1, i, j);
            matrix_get_iter(matrix, &iter2, i/2, j/2+oj);
            matrix->chunks[iter2.chunk][iter2.offset] = tmp->chunks[iter1.chunk][iter1.offset];
        }
    }
    for (i = 1; i < matrix->n_rows; i += 2) {
        for (j = 0; j < matrix->n_columns; j += 2) {
            matrix_get_iter(tmp, &iter1, i, j);
            matrix_get_iter(matrix, &iter2, i/2 + oi, j/2);
            matrix->chunks[iter2.chunk][iter2.offset] = tmp->chunks[iter1.chunk][iter1.offset];
        }
    }
    for (i = 1; i < matrix->n_rows; i += 2) {
        for (j = 1; j < matrix->n_columns; j += 2) {
            matrix_get_iter(tmp, &iter1, i, j);
            matrix_get_iter(matrix, &iter2, i/2 + oi, j/2 + oj);
            matrix->chunks[iter2.chunk][iter2.offset] = tmp->chunks[iter1.chunk][iter1.offset];
        }
    }

    matrix_free(tmp);
}

/* Alternate the signs (-1)^{j-i} */
void matrix_alternate_signs(Matrix *matrix, gboolean do_shift)
{
    guint32 i, j;
    MatrixIter iter;
    guint32 s = do_shift ? 0 : 1;

    for (i = 0; i < matrix->n_rows; ++i) {
        for (j = (i+s)%2; j < matrix->n_columns; j += 2) {
            matrix_get_iter(matrix, &iter, i, j);
            matrix->chunks[iter.chunk][iter.offset] = - matrix->chunks[iter.chunk][iter.offset];
        }
    }
}

void matrix_log_scale(Matrix *matrix)
{
    MatrixIter iter;

    for (matrix_get_iter(matrix, &iter, 0, 0); matrix_iter_is_valid(matrix, &iter); matrix_iter_next(matrix, &iter)) {
        if (matrix->chunks[iter.chunk][iter.offset] > 0)
            matrix->chunks[iter.chunk][iter.offset] = log(matrix->chunks[iter.chunk][iter.offset] + 1.0f);
        else if (matrix->chunks[iter.chunk][iter.offset] < 0)
            matrix->chunks[iter.chunk][iter.offset] = - log( - matrix->chunks[iter.chunk][iter.offset] + 1.0f);
    }
}

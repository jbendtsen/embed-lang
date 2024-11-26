#include "embed.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int IntVector_resize(IntVector *vec, int new_size)
{
    if (new_size <= vec->size)
        return vec->size;

    int new_cap = vec->cap;
    if (new_cap < 16)
        new_cap = 16;
    while (new_cap < new_size)
        new_cap = ((new_cap + 1) * 5) / 3;

    if (new_cap > vec->cap) {
        int *new_buf = malloc(new_cap * sizeof(int));
        if (vec->buf) {
            if (vec->size > 0)
                memcpy(new_buf, vec->buf, vec->size * sizeof(int));
            free(vec->buf);
        }
        vec->buf = new_buf;
        vec->cap = new_cap;
    }

    int old_size = vec->size;
    vec->size = new_size;
    return old_size;
}

void IntVector_add(IntVector *vec, int a)
{
    int pos = vec->size;
    IntVector_resize(vec, pos + 1);
    vec->buf[pos] = a;
}

void IntVector_add_multi(IntVector *vec, int *data, int n_elems)
{
    if (!data || n_elems <= 0)
        return;

    int pos = vec->size;
    IntVector_resize(vec, pos + n_elems);
    for (int i = 0; i < n_elems; i++)
        vec->buf[pos+i] = data[i];
}

void IntVector_set_or_add(IntVector *vec, int idx, int a)
{
    if (idx < 0 || idx >= vec->size)
        IntVector_add(vec, a);
    else
        vec->buf[idx] = a;
}

void IntVector_set_or_add_repeated(IntVector *vec, int idx, int a, int count)
{
    if (count <= 0 || idx < 0)
        return;

    if (idx + count > vec->size)
        IntVector_resize(vec, idx + count);
    for (int i = 0; i < count; i++)
        vec->buf[idx+i] = a;
}

int resolve_line_column_from_index(Buffer *buffer, int *indexes, int *lines, int *cols, int n_indexes)
{
    u8 *buf = buffer->buf;
    int sz = buffer->size;

    // sort indexes with insertion sort, so that 
    for (int i = 1; i < n_indexes; i++) {
        int j = i - 1;
        while (j >= 0 && indexes[j+1] > indexes[j]) {
            int temp = indexes[j+1];
            indexes[j+1] = indexes[j];
            indexes[j] = temp;
            j--;
        }
    }

    int idx = 0;
    int line = 0;
    int col = 0;
    int prev = 0;

    for (int i = 0; i < sz && idx < n_indexes; i++) {
        u8 byte = buf[i];
        int utf8_type = 0;
        if ((byte & 0xe0) == 0xc0)
            utf8_type = 1;
        else if ((byte & 0xf0) == 0xe0)
            utf8_type = 2;
        else if ((byte & 0xf8) == 0xf0)
            utf8_type = 3;
        int cur = byte & ((1 << (7 - utf8_type)) - 1);

        int inc = 0;
        while (utf8_type > 0 && i+inc < sz) {
            cur = (cur << 6) | (buf[i+inc] & 0x3f);
            utf8_type--;
            if (utf8_type > 0)
                inc++;
        }

        while (idx < n_indexes && indexes[idx] == i) {
            lines[idx] = line;
            cols[idx] = col;
            idx++;
        }

        col++;
        if (cur == '\r' || (cur == '\n' && prev != '\r')) {
            line++;
            col = 0;
        }

        prev = cur;
        i += inc;
    }

    return idx;
}

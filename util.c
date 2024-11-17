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

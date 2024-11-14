#pragma once

#define ALLOC_STRUCT(vec, st) IntVector_resize(vec, (vec)->size + (sizeof(st) / sizeof(int)))
#define STRUCT_AT(vec, st, pos) (st*)(&(vec)->buf[pos])
#define COUNT_ALLOCD(size, st) sizeof(int) * (size) / sizeof(st)

typedef unsigned char u8;
typedef unsigned int u32;

typedef struct {
    int *buf;
    int cap;
    int size;
} IntVector;

typedef struct {
    const char *path;
    u8 *buf;
    int size;
} Buffer;

typedef struct {
    short lex_type;
    short builtin_id;
    int depth;
    int left_node;
    int right_node;
    int token_start;
    int token_len;
} Ast_Node;

typedef struct {
    int type;
    int first_node;
} Ast_Statement;

typedef struct {
    IntVector vec;
    int first_statement;
    int buffer_idx;
} Ast;

typedef struct {
    IntVector buffers;
    IntVector asts;
} Project;

int IntVector_resize(IntVector *vec, int new_size);
void IntVector_add(IntVector *vec, int x);
void IntVector_add_multi(IntVector *vec, int *data, int n_elems);

void parse_source_file(Ast *ast, Buffer *buffer, int buffer_idx);

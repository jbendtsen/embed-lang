#pragma once

#define ALLOC_STRUCT(vec_ptr, st) IntVector_resize(vec_ptr, (vec_ptr)->size + (sizeof(st) / sizeof(int)))
#define STRUCT_AT_POS(vec, st, pos) ((st*)(&vec.buf[pos]))
#define STRUCT_AT_INDEX(vec, st, idx) (((st*)vec.buf) + (idx))
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
    short flags;
    short depth;
    char lex_type;
    char precedence;
    short builtin_id;
    int left_node;
    int right_node;
    int next_node;
    int token_start;
    int token_len;
} Ast_Node;

typedef struct {
    union {
        u32 flags;
        int n_members;
    };
    int bytes;
    int token;
} Ast_Type;

typedef struct {
    int module_token;
    int func_token;
    int parent_func_pos;
} Ast_Function;

typedef struct {
    int parent;
    int first_node;
    int left_stmt;
    int next_stmt;
    int right_stmt;
} Ast_Statement;

typedef struct {
    IntVector vec;
    int module_name_token;
    int first_stmt;
    int n_stmts;
    int buffer_idx;
} Ast;

typedef struct {
    IntVector buffers;
    IntVector asts;
} Project;

int IntVector_resize(IntVector *vec, int new_size);
void IntVector_add(IntVector *vec, int x);
void IntVector_add_multi(IntVector *vec, int *data, int n_elems);
void IntVector_set_or_add(IntVector *vec, int idx, int a);
void IntVector_set_or_add_repeated(IntVector *vec, int idx, int a, int count);

int resolve_line_column_from_index(Buffer *buffer, int *indexes, int *lines, int *cols, int n_indexes);

void parse_source_file(Ast *ast, Buffer *buffer, int buffer_idx, IntVector *allocator);

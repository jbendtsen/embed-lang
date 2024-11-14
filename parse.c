#include "embed.h"

#include <stdio.h>
#include <string.h>

#define FLAG_IS_WS               1
#define FLAG_WAS_WS              2
#define FLAG_SHOULD_ADD          4
#define FLAG_END_INCLUSIVE       8
#define FLAG_IN_SINGLE_COMMENT  16
#define FLAG_IN_MULTI_COMMENT   32

#define TOKEN_COMMENT     1
#define TOKEN_STRING      2
#define TOKEN_NUMBER      3
#define TOKEN_IDENTIFIER  4
#define TOKEN_OPERATOR    5

#define ID_SEMICOLON  1

void lex_source(Ast *ast, Buffer *buffer)
{
    char token_dbg[64];

    u8 *buf = buffer->buf;
    int sz = buffer->size;

    u32 flags = FLAG_WAS_WS;
    int start = -1;
    int cur = 0;
    int prev = 0;
    int prev_prev = 0;
    int type = 0;
    int old_type = 0;
    int quote_char = 0;

    int i = 0;
    while (i < sz) {
        u8 byte = buf[i];
        int utf8_type = 0;
        if ((byte & 0xe0) == 0xc0)
            utf8_type = 1;
        else if ((byte & 0xf0) == 0xe0)
            utf8_type = 2;
        else if ((byte & 0xf8) == 0xf0)
            utf8_type = 3;
        cur = byte & ((1 << (7 - utf8_type)) - 1);

        while (utf8_type > 0 && i < sz) {
            cur = (cur << 6) | (buf[i] & 0x3f);
            utf8_type--;
            if (utf8_type > 0)
                i++;
        }

        flags &= ~(FLAG_IS_WS | FLAG_SHOULD_ADD);
        flags |= -(u32)(cur == '\t' || cur == '\n' || cur == '\r' || cur == ' ') & FLAG_IS_WS;

        if (flags & FLAG_IN_SINGLE_COMMENT) {
            if (cur == '\n') {
                flags ^= FLAG_IN_SINGLE_COMMENT;
                flags |= FLAG_SHOULD_ADD;
            }
            goto lex_next;
        }
        if (flags & FLAG_IN_MULTI_COMMENT) {
            if (prev == '*' && cur == '/') {
                flags ^= FLAG_IN_MULTI_COMMENT;
                flags |= FLAG_SHOULD_ADD | FLAG_END_INCLUSIVE;
            }
            goto lex_next;
        }
        if (quote_char) {
            if (!(prev_prev != '\\' && prev == '\\') && cur == quote_char) {
                quote_char = 0;
                flags |= FLAG_SHOULD_ADD | FLAG_END_INCLUSIVE;
            }
            goto lex_next;
        }
        if (cur == '\'' || cur == '"') {
            quote_char = cur;
            type = TOKEN_STRING;
            start = i;
            goto lex_next;
        }
        if (prev == '/' && cur == '/') {
            flags |= FLAG_IN_SINGLE_COMMENT;
            type = TOKEN_COMMENT;
            goto lex_next;
        }
        if (prev == '/' && cur == '*') {
            flags |= FLAG_IN_MULTI_COMMENT;
            type = TOKEN_COMMENT;
            goto lex_next;
        }

        if ((flags & FLAG_IS_WS) == 0 && (cur < ' ' || cur > '~')) {
            goto lex_next;
        }

        if ((flags & FLAG_IS_WS) == 0) {
            if (cur == '#' || cur == '@' || cur == '_' || (cur >= 'A' && cur <= 'Z') || (cur >= 'a' && cur <= 'z')) {
                type = TOKEN_IDENTIFIER;
            }
            else if (cur >= '0' && cur <= '9') {
                if (old_type != TOKEN_IDENTIFIER)
                    type = TOKEN_NUMBER;
            }
            else {
                type = TOKEN_OPERATOR;
                flags |= FLAG_SHOULD_ADD & -(u32)(
                    prev == '(' || prev == ')' || prev == '[' || prev == ']' || prev == ';' ||
                    cur == ';' ||
                    (prev == '=' && cur != '=')
                );
            }

            if (flags & FLAG_WAS_WS)
                start = i;
            else if (type != old_type && !(old_type == TOKEN_NUMBER && type == TOKEN_IDENTIFIER))
                flags |= FLAG_SHOULD_ADD;
        }
        else {
            type = 0;
            if ((flags & FLAG_WAS_WS) == 0)
                flags |= FLAG_SHOULD_ADD;
        }

lex_next:
        if (start == i)
            old_type = type;

        if (((flags & FLAG_SHOULD_ADD) || i >= sz-1) && old_type > 0) {
            int inc = start == i || (flags & FLAG_END_INCLUSIVE) != 0;
            flags &= ~FLAG_END_INCLUSIVE;

            int len = i - start + inc;
            short id = 0;
            if (len == 1) {
                if (len == 1 && buf[start] == ';')
                    id = ID_SEMICOLON;
                else if (len == 1 && buf[start] == ',')
                    id = ID_COMMA;
                else if (len == 1 && buf[start] == '=')
                    id = ID_EQUALS;
                else if (len == 1 && buf[start] == '(')
                    id = ID_PAREN_OPEN;
                else if (len == 1 && buf[start] == ')')
                    id = ID_PAREN_CLOSE;
                else if (len == 1 && buf[start] == '[')
                    id = ID_SQUARE_OPEN;
                else if (len == 1 && buf[start] == ']')
                    id = ID_SQUARE_CLOSE;
                else if (len == 1 && buf[start] == '{')
                    id = ID_BRACE_OPEN;
                else if (len == 1 && buf[start] == '}')
                    id = ID_BRACE_CLOSE;
                else if (len == 1 && buf[start] == '.')
                    id = ID_DOT;
                else if (len == 1 && buf[start] == '<')
                    id = ID_LESS_TEMPL;
                else if (len == 1 && buf[start] == '>')
                    id = ID_GREATER_TEMPL;
                else if (len == 1 && buf[start] == '+')
                    id = ID_PLUS_POS;
                else if (len == 1 && buf[start] == '-')
                    id = ID_MINUS_NEG;
                else if (len == 1 && buf[start] == '*')
                    id = ID_MULTIPLY;
                else if (len == 1 && buf[start] == '/')
                    id = ID_DIVIDE;
                else if (len == 1 && buf[start] == '^')
                    id = ID_XOR_REFER;
                else if (len == 1 && buf[start] == '|')
                    id = ID_OR_OP;
                else if (len == 1 && buf[start] == '&')
                    id = ID_AND_OP;
                else if (len == 1 && buf[start] == '~')
                    id = ID_NOT_OP;
                else if (len == 1 && buf[start] == '!')
                    id = ID_NOT_BOOL;
            }
            else if (len == 2) {
                if (buf[start] == 'i' && buf[start+1] == 'f')
                    id = ID_IF;
                else if (buf[start] == '|' && buf[start+1] == '<')
                    id = ID_LEFT_SHIFT;
                else if (buf[start] == '|' && buf[start+1] == '>')
                    id = ID_RIGHT_SHIFT;
                else if (buf[start] == '|' && buf[start+1] == '|')
                    id = ID_OR_BOOL;
                else if (buf[start] == '&' && buf[start+1] == '&')
                    id = ID_AND_BOOL;
            }
            else if (len == 3) {
                else if (buf[start] == '|' && buf[start+1] == '<' && buf[start+2] == '=')
                    id = ID_LEFT_SHIFT_ASSIGN;
                else if (buf[start] == '|' && buf[start+1] == '>' && buf[start+2] == '=')
                    id = ID_RIGHT_SHIFT_ASSIGN;
            }
            else if (len == 4) {
            }

            int pos = ALLOC_STRUCT(&ast->vec, Ast_Node);
            *STRUCT_AT(&ast->vec, Ast_Node, pos) = (Ast_Node) {
                .lex_type = (short)old_type,
                .builtin_id = id,
                .depth = 0,
                .left_node = 0,
                .right_node = 0,
                .token_start = start,
                .token_len = len
            };

            if (0) {
                int dbg_len = i - start + inc;
                if (dbg_len > 63) dbg_len = 63;
                memcpy(token_dbg, &buf[start], dbg_len);
                token_dbg[dbg_len] = 0;
                printf("%c: \"%s\"\n", "CSNIO"[old_type-1], token_dbg);
            }

            start = i + inc;
        }

        flags &= ~(FLAG_SHOULD_ADD | FLAG_WAS_WS);
        flags |= -(flags & FLAG_IS_WS) & FLAG_WAS_WS;
        old_type = type;
        prev_prev = prev;
        prev = cur;
        i++;
    }
}

void parse_source_file(Ast *ast, Buffer *buffer, int buffer_idx)
{
    *ast = (Ast) {0};
    ast->buffer_idx = buffer_idx;
    lex_source(ast, buffer);

    int first_statement = ast->vec.size;
    int n_nodes = COUNT_ALLOCD(first_statement, Ast_Node);

    for (int i = 0; i < n_nodes; i++) {
        Ast_Node *node = STRUCT_AT(&ast->vec, Ast_Node, i * sizeof(Ast_Node) / sizeof(int));
        node->
    }
}

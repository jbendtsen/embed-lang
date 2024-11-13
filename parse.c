#include "embed.h"

#define FLAG_WAS_WS             1
#define FLAG_END_INCLUSIVE      2
#define FLAG_IN_SINGLE_COMMENT  4
#define FLAG_IN_MULTI_COMMENT   8

#define TOKEN_COMMENT     1
#define TOKEN_STRING      2
#define TOKEN_NUMBER      3
#define TOKEN_IDENTIFIER  4
#define TOKEN_OPERATOR    5

void parse_source_file(Ast *ast, Buffer *buffer, int buffer_idx)
{
    u8 *buf = buffer->buf;
    int sz = buffer->size;

    u32 flags = FLAG_WAS_WS;
    int start = -1;
    int cur = 0;
    int prev = 0;
    int type = 0;
    int quote_char = 0;

    for (int i = 0; i < sz; i++) {
        u8 byte = buf[i];
        int type = 0;
        if ((byte & 0xe0) == 0xc0)
            type = 1;
        else if ((byte & 0xf0) == 0xe0)
            type = 2;
        else if ((byte & 0xf8) == 0xf0)
            type = 3;
        cur = byte & ((1 << (7 - type)) - 1);

        while (type > 0 && i < n_bytes) {
            cur = (cur << 6) | (buf[i] & 0x3f);
            type--;
            if (type > 0)
                i++;
        }

        bool is_ws = cur == '\t' || cur == '\n' || cur == '\r' || cur == ' ';
        bool should_add = false;

        if (flags & FLAG_IN_SINGLE_COMMENT) {
            if (cur == '\n') {
                flags ^= FLAG_IN_SINGLE_COMMENT;
                type = TOKEN_COMMENT;
                should_add = true;
            }
            goto lex_next;
        }
        if (flags & FLAG_IN_MULTI_COMMENT) {
            if (prev == '*' && cur == '/') {
                flags ^= FLAG_IN_MULTI_COMMENT;
                flags |= FLAG_END_INCLUSIVE;
                type = TOKEN_COMMENT;
                should_add = true;
            }
            goto lex_next;
        }
        if (quote_char && cur == quote_char) {
            quote_char = 0;
            flags |= FLAG_END_INCLUSIVE;
            type = TOKEN_STRING;
            should_add = true;
            goto lex_next;
        }
        if (cur == '\'' || cur == '"') {
            quote_char = cur;
            goto lex_next;
        }
        if (prev == '/' && cur == '/') {
            flags |= FLAG_IN_SINGLE_COMMENT;
            goto lex_next;
        }
        if (prev == '/' && cur == '*') {
            flags |= FLAG_IN_MULTI_COMMENT;
            goto lex_next;
        }

        if (!is_ws && (cur < ' ' || cur > '`')) {
            goto lex_next;
        }

        if (!is_ws && (flags & FLAG_WAS_WS)) {
            if (cur == '_' || (cur >= 'A' && cur <= 'Z') || (cur >= 'a' && cur <= 'z'))
                type = TYPE_IDENTIFIER;
            else if (cur >= '0' && cur <= '9')
                type = TYPE_NUMBER;
            else
                type = TYPE_STRING;
            start = i;
        }
        if (is_ws && (flags & FLAG_WAS_WS) == 0) {
            should_add = true;
        }

lex_next:
        if (should_add) {
            int inc = (flags & FLAG_END_INCLUSIVE) != 0;
            flags &= ~FLAG_END_INCLUSIVE;

            int pos = ALLOC_STRUCT(&ast->vec, Ast_Node);
            *STRUCT_AT(&ast->vec, Ast_Node, pos) = (Ast_Node) {
                .type = type,
                .left_node = 0,
                .right_node = 0,
                .token_start = start,
                .token_len = i - start + inc
            };

            start = i;
            should_add = false;
        }

        flags = (flags & ~FLAG_WAS_WS) | (-(u32)is_ws & FLAG_WAS_WS);
        prev = cur;
    }
}

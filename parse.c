#include "embed.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

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

    bool is_ws = false;
    bool should_add = false;

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

        is_ws = cur == '\t' || cur == '\n' || cur == '\r' || cur == ' ';
        should_add = false;

        if (flags & FLAG_IN_SINGLE_COMMENT) {
            if (cur == '\n') {
                flags ^= FLAG_IN_SINGLE_COMMENT;
                should_add = true;
            }
            goto lex_next;
        }
        if (flags & FLAG_IN_MULTI_COMMENT) {
            if (prev == '*' && cur == '/') {
                flags ^= FLAG_IN_MULTI_COMMENT;
                flags |= FLAG_END_INCLUSIVE;
                should_add = true;
            }
            goto lex_next;
        }
        if (quote_char) {
            if (!(prev_prev != '\\' && prev == '\\') && cur == quote_char) {
                quote_char = 0;
                flags |= FLAG_END_INCLUSIVE;
                should_add = true;
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

        if (!is_ws && (cur < ' ' || cur > '~')) {
            goto lex_next;
        }

        if (!is_ws) {
            if (cur == '_' || (cur >= 'A' && cur <= 'Z') || (cur >= 'a' && cur <= 'z')) {
                type = TOKEN_IDENTIFIER;
            }
            else if (cur >= '0' && cur <= '9') {
                if (old_type != TOKEN_IDENTIFIER)
                    type = TOKEN_NUMBER;
            }
            else {
                type = TOKEN_OPERATOR;
            }

            if (flags & FLAG_WAS_WS)
                start = i;
            else if (type != old_type && !(old_type == TOKEN_NUMBER && type == TOKEN_IDENTIFIER))
                should_add = true;

            if (i == 1) {
                printf("SECOND CHARACTER: cur = %d, is_ws = %d, flags = 0x%x, old_type = %d, type = %d, start = %d, should_add = %d\n", cur, (int)is_ws, flags, old_type, type, start, (int)should_add);
            }
        }
        else {
            type = 0;
            if ((flags & FLAG_WAS_WS) == 0)
                should_add = true;
        }

lex_next:
        if (start == i)
            old_type = type;

        if ((should_add || i >= sz-1) && old_type > 0) {
            int inc = start == i || (flags & FLAG_END_INCLUSIVE) != 0;
            flags &= ~FLAG_END_INCLUSIVE;

            int pos = ALLOC_STRUCT(&ast->vec, Ast_Node);
            *STRUCT_AT(&ast->vec, Ast_Node, pos) = (Ast_Node) {
                .type = old_type,
                .left_node = 0,
                .right_node = 0,
                .token_start = start,
                .token_len = i - start + inc
            };

            if (true) {
                int dbg_len = i - start + inc;
                if (dbg_len > 63) dbg_len = 63;
                memcpy(token_dbg, &buf[start], dbg_len);
                token_dbg[dbg_len] = 0;
                printf("%c: \"%s\"\n", "CSNIO"[old_type-1], token_dbg);
            }

            start = i + inc;
        }

        should_add = false;
        flags = (flags & ~FLAG_WAS_WS) | (-(u32)is_ws & FLAG_WAS_WS);
        old_type = type;
        prev_prev = prev;
        prev = cur;
        i++;
    }
}

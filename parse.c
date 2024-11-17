#include "embed.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define ARR_EQUAL_2(a, b) a[0] == b[0] && a[1] == b[1]
#define ARR_EQUAL_3(a, b) ARR_EQUAL_2(a, b) && a[2] == b[2]
#define ARR_EQUAL_4(a, b) ARR_EQUAL_3(a, b) && a[3] == b[3]
#define ARR_EQUAL_5(a, b) ARR_EQUAL_4(a, b) && a[4] == b[4]
#define ARR_EQUAL_6(a, b) ARR_EQUAL_5(a, b) && a[5] == b[5]
#define ARR_EQUAL_7(a, b) ARR_EQUAL_6(a, b) && a[6] == b[6]

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

#define ID_SEMICOLON 1
#define ID_COMMA 2
#define ID_COLON 3
#define ID_EQUALS 4
#define ID_PAREN_OPEN 5
#define ID_PAREN_CLOSE 6
#define ID_SQUARE_OPEN 7
#define ID_SQUARE_CLOSE 8
#define ID_BRACE_OPEN 9
#define ID_BRACE_CLOSE 10
#define ID_DOT 11
#define ID_LESS_THAN 12
#define ID_GREATER_THAN 13
#define ID_PLUS_POS 14
#define ID_MINUS_NEG 15
#define ID_MULTIPLY 16
#define ID_DIVIDE 17
#define ID_MODULO 18
#define ID_XOR_REFER 19
#define ID_OR_OP 20
#define ID_AND_OP 21
#define ID_NOT_OP 22
#define ID_NOT_BOOL 23
#define ID_AUTO_ASSIGN 24
#define ID_LESS_EQUAL 25
#define ID_GREATER_EQUAL 26
#define ID_PLUS_ASSIGN 27
#define ID_MINUS_ASSIGN 28
#define ID_MULTIPLY_ASSIGN 29
#define ID_DIVIDE_ASSIGN 30
#define ID_MODULO_ASSIGN 31
#define ID_XOR_ASSIGN 32
#define ID_OR_ASSIGN 33
#define ID_AND_ASSIGN 34
#define ID_NOT_EQUAL 35
#define ID_IS_EQUAL 36
#define ID_IF 37
#define ID_LEFT_SHIFT 38
#define ID_RIGHT_SHIFT 39
#define ID_TEMPL_OPEN 40
#define ID_TEMPL_CLOSE 41
#define ID_OR_BOOL 42
#define ID_AND_BOOL 43
#define ID_COMMA_OPTIONAL 44
#define ID_FOR 45
#define ID_LEFT_SHIFT_ASSIGN 46
#define ID_RIGHT_SHIFT_ASSIGN 47
#define ID_ELSE 48
#define ID_CASE 49
#define ID_GOTO 50
#define ID_FUNC 51
#define ID_ASM 52
#define ID_MACRO 53
#define ID_CONST 54
#define ID_BREAK 55
#define ID_ONLY 56
#define ID_RETURN 57
#define ID_REPEAT 58
#define ID_SWITCH 59
#define ID_MODULE 60
#define ID_IMPORT 61
#define ID_KNOWN 62
#define ID_DEFAULT 63
#define ID_BITCAST 64
#define ID_COMP_IMPORT 65
#define ID_CONVENTION 66
#define ID_FALLTHROUGH 67

#define OPERATOR(symbol, prec) (symbol) | ((prec) << 16)

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
    int last_lex_type = 0;
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

            char *s = (char*)&buf[start];
            int len = i - start + inc;
            int id = 0;
            bool is_binary = last_lex_type == TOKEN_NUMBER || last_lex_type == TOKEN_IDENTIFIER;
            if (len == 1) {
                switch (*s) {
                    case ';': id = OPERATOR(ID_SEMICOLON, 20); break;
                    case ',': id = OPERATOR(ID_COMMA, 19); break;
                    case ':': id = OPERATOR(ID_COLON, 18); break;
                    case '=': id = OPERATOR(ID_EQUALS, 17); break;
                    case '(': id = OPERATOR(ID_PAREN_OPEN, 3); break;
                    case ')': id = OPERATOR(ID_PAREN_CLOSE, 3); break;
                    case '[': id = OPERATOR(ID_SQUARE_OPEN, 2); break;
                    case ']': id = OPERATOR(ID_SQUARE_CLOSE, 2); break;
                    case '{': id = OPERATOR(ID_BRACE_OPEN, 20); break;
                    case '}': id = OPERATOR(ID_BRACE_CLOSE, 20); break;
                    case '.': id = OPERATOR(ID_DOT, 1); break;
                    case '<': id = OPERATOR(ID_LESS_THAN, 11); break;
                    case '>': id = OPERATOR(ID_GREATER_THAN, 11); break;
                    case '+': id = OPERATOR(ID_PLUS_POS, is_binary ? 6 : 4); break;
                    case '-': id = OPERATOR(ID_MINUS_NEG, is_binary ? 6 : 4); break;
                    case '*': id = OPERATOR(ID_MULTIPLY, is_binary ? 5 : 4); break;
                    case '/': id = OPERATOR(ID_DIVIDE, 5); break;
                    case '%': id = OPERATOR(ID_MODULO, 5); break;
                    case '^': id = OPERATOR(ID_XOR_REFER, is_binary ? 9 : 4); break;
                    case '|': id = OPERATOR(ID_OR_OP, 10); break;
                    case '&': id = OPERATOR(ID_AND_OP, 8); break;
                    case '~': id = OPERATOR(ID_NOT_OP, 4); break;
                    case '!': id = OPERATOR(ID_NOT_BOOL, 4); break;
                }
            }
            else if (len == 2 && s[1] == '=') {
                switch (*s) {
                    case ':': id = OPERATOR(ID_AUTO_ASSIGN, 17); break;
                    case '<': id = OPERATOR(ID_LESS_EQUAL, 11); break;
                    case '>': id = OPERATOR(ID_GREATER_EQUAL, 11); break;
                    case '+': id = OPERATOR(ID_PLUS_ASSIGN, 17); break;
                    case '-': id = OPERATOR(ID_MINUS_ASSIGN, 17); break;
                    case '*': id = OPERATOR(ID_MULTIPLY_ASSIGN, 17); break;
                    case '/': id = OPERATOR(ID_DIVIDE_ASSIGN, 17); break;
                    case '%': id = OPERATOR(ID_MODULO_ASSIGN, 17); break;
                    case '^': id = OPERATOR(ID_XOR_ASSIGN, 17); break;
                    case '|': id = OPERATOR(ID_OR_ASSIGN, 17); break;
                    case '&': id = OPERATOR(ID_AND_ASSIGN, 17); break;
                    case '!': id = OPERATOR(ID_NOT_EQUAL, 12); break;
                    case '=': id = OPERATOR(ID_IS_EQUAL, 12); break;
                }
            }
            else if (len == 2) {
                if (ARR_EQUAL_2(s, "if"))
                    id = ID_IF;
                else if (ARR_EQUAL_2(s, "<|"))
                    id = OPERATOR(ID_LEFT_SHIFT, 7);
                else if (ARR_EQUAL_2(s, ">|"))
                    id = OPERATOR(ID_RIGHT_SHIFT, 7);
                else if (ARR_EQUAL_2(s, "<<"))
                    id = OPERATOR(ID_TEMPL_OPEN, 1);
                else if (ARR_EQUAL_2(s, ">>"))
                    id = OPERATOR(ID_TEMPL_CLOSE, 1);
                else if (ARR_EQUAL_2(s, "||"))
                    id = OPERATOR(ID_OR_BOOL, 14);
                else if (ARR_EQUAL_2(s, "&&"))
                    id = OPERATOR(ID_AND_BOOL, 13);
                else if (ARR_EQUAL_2(s, "/,"))
                    id = OPERATOR(ID_COMMA_OPTIONAL, 19);
            }
            else if (len == 3) {
                if (ARR_EQUAL_3(s, "for"))
                    id = ID_FOR;
                else if (ARR_EQUAL_3(s, "<|="))
                    id = OPERATOR(ID_LEFT_SHIFT_ASSIGN, 17);
                else if (ARR_EQUAL_3(s, ">|="))
                    id = OPERATOR(ID_RIGHT_SHIFT_ASSIGN, 17);
            }
            else if (len == 4) {
                if (ARR_EQUAL_4(s, "else"))
                    id = ID_ELSE;
                else if (ARR_EQUAL_4(s, "case"))
                    id = ID_CASE;
                else if (ARR_EQUAL_4(s, "goto"))
                    id = ID_GOTO;
                else if (ARR_EQUAL_4(s, "func"))
                    id = ID_FUNC;
                else if (ARR_EQUAL_4(s, "#asm"))
                    id = ID_ASM;
            }
            else if (len == 5) {
                if (ARR_EQUAL_5(s, "macro"))
                    id = ID_MACRO;
                else if (ARR_EQUAL_5(s, "const"))
                    id = ID_CONST;
                else if (ARR_EQUAL_5(s, "break"))
                    id = ID_BREAK;
                else if (ARR_EQUAL_5(s, "#only"))
                    id = ID_ONLY;
            }
            else if (len == 6) {
                if (ARR_EQUAL_6(s, "return"))
                    id = ID_RETURN;
                else if (ARR_EQUAL_6(s, "repeat"))
                    id = ID_REPEAT;
                else if (ARR_EQUAL_6(s, "switch"))
                    id = ID_SWITCH;
                else if (ARR_EQUAL_6(s, "module"))
                    id = ID_MODULE;
                else if (ARR_EQUAL_6(s, "import"))
                    id = ID_IMPORT;
                else if (ARR_EQUAL_6(s, "#known"))
                    id = ID_KNOWN;
            }
            else if (len == 7) {
                if (ARR_EQUAL_7(s, "default"))
                    id = ID_DEFAULT;
                else if (ARR_EQUAL_7(s, "bitcast"))
                    id = ID_BITCAST;
                else if (ARR_EQUAL_7(s, "#import"))
                    id = ID_COMP_IMPORT;
            }
            else if (len == 11 && !memcmp(s, "#convention", 11)) {
                id = ID_CONVENTION;
            }
            else if (len == 11 && !memcmp(s, "fallthrough", 11)) {
                id = ID_FALLTHROUGH;
            }

            int pos = ALLOC_STRUCT(&ast->vec, Ast_Node);
            *STRUCT_AT_POS(ast->vec, Ast_Node, pos) = (Ast_Node) {
                .lex_type = (char)old_type,
                .precedence = (char)((id >> 16) & 0x7f),
                .builtin_id = (short)(id & 0x7fff),
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
            if (old_type != TOKEN_COMMENT)
                last_lex_type = old_type;
        }

        flags &= ~(FLAG_SHOULD_ADD | FLAG_WAS_WS);
        flags |= -(flags & FLAG_IS_WS) & FLAG_WAS_WS;
        old_type = type;
        prev_prev = prev;
        prev = cur;
        i++;
    }
}

void sort_precedence(int *idx, Ast_Node *node, int n_nodes)
{
    for (int i = 1; i < n_nodes; i++) {
        int j = i - 1;
        while (j >= 0 && node[idx[j+1]].precedence > node[idx[j]].precedence) {
            int temp = idx[j+1];
            idx[j+1] = idx[j];
            idx[j] = temp;
            j--;
        }
    }
}

void parse_source_file(Ast *ast, Buffer *buffer, int buffer_idx, IntVector *allocator)
{
    char token_dbg[64];

    *ast = (Ast) {0};
    ast->buffer_idx = buffer_idx;
    lex_source(ast, buffer);

    int first_statement = ast->vec.size;
    int n_nodes = COUNT_ALLOCD(first_statement, Ast_Node);

    for (int i = 0; i < allocator->size; i++)
        allocator->buf[i] = i;

    for (int i = 0; i < n_nodes; i++) {
        Ast_Node *node = STRUCT_AT_INDEX(ast->vec, Ast_Node, i);
        int end = 0;
        int n_ops = 0;
        while (i + end < n_nodes) {
            short op = node[end].builtin_id;
            if (op == ID_SEMICOLON || op == ID_COMMA ||
                op == ID_BRACE_OPEN || op == ID_BRACE_CLOSE ||
                op == ID_PAREN_OPEN || op == ID_PAREN_CLOSE ||
                op == ID_SQUARE_OPEN || op == ID_SQUARE_CLOSE ||
                op == ID_TEMPL_OPEN || op == ID_TEMPL_CLOSE
            )
                break;

            n_ops += node[end].precedence > 0;
            IntVector_set_or_add(allocator, end, end);
            end++;
        }
        sort_precedence(allocator->buf, node, end);

        for (int i = 0; i < n_ops; i++) {
            
        }

        if (1) {
            //printf("%d, %d -> ", i, end);
            for (int j = 0; j < end; j++) {
                Ast_Node *n = &node[allocator->buf[j]];
                int dbg_len = n->token_len;
                if (dbg_len > 63) dbg_len = 63;
                memcpy(token_dbg, &buffer->buf[n->token_start], dbg_len);
                token_dbg[dbg_len] = 0;
                printf("%s ", token_dbg);
            }
            putchar('\n');
        }

        i += end;
    }
}

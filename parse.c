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
#define FLAG_INSERTED_PAREN     64

#define FLAG_VISITED  1

#define FLAG_TYPE_MACRO  1
#define FLAG_TYPE_FUNC   2

#define TOKEN_COMMENT     1
#define TOKEN_STRING      2
#define TOKEN_NUMBER      3
#define TOKEN_IDENTIFIER  4
#define TOKEN_OPERATOR    5
#define TOKEN_UNARY       6

#define ID_IDENTIFIER_BIND 1
#define ID_SEMICOLON 2
#define ID_COMMA 3
#define ID_COLON 4
#define ID_EQUALS 5
#define ID_INVOKE_OPEN 6
#define ID_PAREN_OPEN 7
#define ID_PAREN_CLOSE 8
#define ID_SQUARE_OPEN 9
#define ID_SQUARE_CLOSE 10
#define ID_BRACE_OPEN 11
#define ID_BRACE_CLOSE 12
#define ID_DOT 13
#define ID_LESS_THAN 14
#define ID_GREATER_THAN 15
#define ID_ADD 16
#define ID_POSITIVE 17
#define ID_SUBTRACT 18
#define ID_NEGATIVE 19
#define ID_MULTIPLY 20
#define ID_DEREFERENCE 21
#define ID_DIVIDE 22
#define ID_MODULO 23
#define ID_POLYMORPHIC 24
#define ID_NOTE 25
#define ID_REFER_INDEX 26
#define ID_XOR_OP 27
#define ID_REFERENCE 28
#define ID_OR_OP 29
#define ID_AND_OP 30
#define ID_NOT_OP 31
#define ID_NOT_BOOL 32
#define ID_LESS_EQUAL 33
#define ID_GREATER_EQUAL 34
#define ID_ADD_ASSIGN 35
#define ID_SUBTRACT_ASSIGN 36
#define ID_MULTIPLY_ASSIGN 37
#define ID_DIVIDE_ASSIGN 38
#define ID_MODULO_ASSIGN 39
#define ID_XOR_ASSIGN 40
#define ID_OR_ASSIGN 41
#define ID_AND_ASSIGN 42
#define ID_NOT_EQUAL 43
#define ID_IS_EQUAL 44
#define ID_IF 45
#define ID_LEFT_SHIFT 46
#define ID_RIGHT_SHIFT 47
#define ID_TEMPL_OPEN 48
#define ID_TEMPL_CLOSE 49
#define ID_REFER_BYTE 50
#define ID_OTHERWISE 51
#define ID_POSTFIX_INCREMENT 52
#define ID_PREFIX_INCREMENT 53
#define ID_POSTFIX_DECREMENT 54
#define ID_PREFIX_DECREMENT 55
#define ID_OR_BOOL 56
#define ID_AND_BOOL 57
#define ID_COMMA_OPTIONAL 58
#define ID_VAR 59
#define ID_FOR 60
#define ID_ABS 61
#define ID_MIN 62
#define ID_MAX 63
#define ID_LEFT_SHIFT_ASSIGN 64
#define ID_RIGHT_SHIFT_ASSIGN 65
#define ID_ELSE 66
#define ID_CASE 67
#define ID_GOTO 68
#define ID_FUNC 69
#define ID_ASM 70
#define ID_MACRO 71
#define ID_BREAK 72
#define ID_ONLY 73
#define ID_RETURN 74
#define ID_REPEAT 75
#define ID_SWITCH 76
#define ID_INLINE 77
#define ID_MODULE 78
#define ID_IMPORT 79
#define ID_DECIDE 80
#define ID_KNOWN 81
#define ID_DEFAULT 82
#define ID_BITCAST 83
#define ID_COMP_IMPORT 84
#define ID_CONVENTION 85
#define ID_FALLTHROUGH 86

#define OPERATOR(symbol, prec) (symbol) | ((prec) << 16)

int lex_source(Ast *ast, Buffer *buffer)
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
    int last_id = 0;
    int last_identifier_pos = -1;
    int quote_char = 0;
    int module = -1;

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
            if (cur == '#' || cur == '_' || (cur >= 'A' && cur <= 'Z') || (cur >= 'a' && cur <= 'z')) {
                type = TOKEN_IDENTIFIER;
            }
            else if (cur >= '0' && cur <= '9') {
                if (old_type != TOKEN_IDENTIFIER)
                    type = TOKEN_NUMBER;
            }
            else {
                type = TOKEN_OPERATOR;
                flags |= FLAG_SHOULD_ADD & -(u32)(
                    //prev == '(' || prev == ')' || prev == '[' || prev == ']' || prev == ';' ||
                    cur == ';' || cur == ',' || cur == '.' || cur == '~' ||
                    cur == '(' || cur == ')' || cur == '[' || cur == ']' ||
                    cur == '{' || cur == '}' ||
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
        if (start == i) {
            old_type = type;
            flags &= ~FLAG_SHOULD_ADD; // & -(u32)(type != TOKEN_OPERATOR);
        }

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
                    case '(': id = OPERATOR(last_lex_type == TOKEN_IDENTIFIER ? ID_INVOKE_OPEN : ID_PAREN_OPEN, 3); break;
                    case ')': id = OPERATOR(ID_PAREN_CLOSE, 3); break;
                    case '[': id = OPERATOR(ID_SQUARE_OPEN, 2); break;
                    case ']': id = OPERATOR(ID_SQUARE_CLOSE, 2); break;
                    case '{': id = OPERATOR(ID_BRACE_OPEN, 2); break;
                    case '}': id = OPERATOR(ID_BRACE_CLOSE, 2); break;
                    case '.': id = OPERATOR(ID_DOT, 2); break;
                    case '<': id = OPERATOR(ID_LESS_THAN, 11); break;
                    case '>': id = OPERATOR(ID_GREATER_THAN, 11); break;
                    case '+': id = OPERATOR(is_binary ? ID_ADD : ID_POSITIVE, is_binary ? 6 : 4); break;
                    case '-': id = OPERATOR(is_binary ? ID_SUBTRACT : ID_NEGATIVE, is_binary ? 6 : 4); break;
                    case '*': id = OPERATOR(is_binary ? ID_MULTIPLY : ID_DEREFERENCE, is_binary ? 5 : 4); break;
                    case '/': id = OPERATOR(ID_DIVIDE, 5); break;
                    case '%': id = OPERATOR(ID_MODULO, 5); break;
                    case '$': id = OPERATOR(ID_POLYMORPHIC, 1); break;
                    case '?': id = OPERATOR(ID_NOTE, 1); break;
                    case '@': id = OPERATOR(ID_REFER_INDEX, 7); break;
                    case '^': id = OPERATOR(is_binary ? ID_XOR_OP : ID_REFERENCE, is_binary ? 9 : 4); break;
                    case '|': id = OPERATOR(ID_OR_OP, 10); break;
                    case '&': id = OPERATOR(ID_AND_OP, 8); break;
                    case '~': id = OPERATOR(ID_NOT_OP, 4); break;
                    case '!': id = OPERATOR(ID_NOT_BOOL, 4); break;
                }
            }
            else if (len == 2 && s[1] == '=') {
                switch (*s) {
                    case '<': id = OPERATOR(ID_LESS_EQUAL, 11); break;
                    case '>': id = OPERATOR(ID_GREATER_EQUAL, 11); break;
                    case '+': id = OPERATOR(ID_ADD_ASSIGN, 17); break;
                    case '-': id = OPERATOR(ID_SUBTRACT_ASSIGN, 17); break;
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
                    id = OPERATOR(ID_TEMPL_OPEN, 2);
                else if (ARR_EQUAL_2(s, ">>"))
                    id = OPERATOR(ID_TEMPL_CLOSE, 2);
                else if (ARR_EQUAL_2(s, "@@"))
                    id = OPERATOR(ID_REFER_BYTE, 7);
                else if (ARR_EQUAL_2(s, "??"))
                    id = OPERATOR(ID_OTHERWISE, 1);
                else if (ARR_EQUAL_2(s, "++"))
                    id = OPERATOR(last_lex_type == TOKEN_IDENTIFIER ? ID_POSTFIX_INCREMENT : ID_PREFIX_INCREMENT, 4);
                else if (ARR_EQUAL_2(s, "--"))
                    id = OPERATOR(last_lex_type == TOKEN_IDENTIFIER ? ID_POSTFIX_DECREMENT : ID_PREFIX_DECREMENT, 4);
                else if (ARR_EQUAL_2(s, "||"))
                    id = OPERATOR(ID_OR_BOOL, 14);
                else if (ARR_EQUAL_2(s, "&&"))
                    id = OPERATOR(ID_AND_BOOL, 13);
                else if (ARR_EQUAL_2(s, "/,"))
                    id = OPERATOR(ID_COMMA_OPTIONAL, 19);
            }
            else if (len == 3) {
                if (ARR_EQUAL_3(s, "var"))
                    id = ID_VAR;
                else if (ARR_EQUAL_3(s, "for"))
                    id = ID_FOR;
                else if (ARR_EQUAL_3(s, "abs"))
                    id = ID_ABS;
                else if (ARR_EQUAL_3(s, "min"))
                    id = ID_MIN;
                else if (ARR_EQUAL_3(s, "max"))
                    id = ID_MAX;
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
                else if (ARR_EQUAL_6(s, "inline"))
                    id = ID_INLINE;
                else if (ARR_EQUAL_6(s, "module"))
                    id = ID_MODULE;
                else if (ARR_EQUAL_6(s, "import"))
                    id = ID_IMPORT;
                else if (ARR_EQUAL_6(s, "decide"))
                    id = ID_DECIDE;
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

            if (last_lex_type == 0 && old_type != TOKEN_COMMENT && id != ID_MODULE) {
                printf(
                    "Each source file must start with the module keyword, eg:\n"
                    "module foo;\n"
                );
                return -1;
            }

            int pos;
            if ((flags & FLAG_INSERTED_PAREN) && ((id & 0xffff) == ID_BRACE_OPEN || (id & 0xffff) == ID_BRACE_CLOSE || (id & 0xffff) == ID_SEMICOLON)) {
                pos = ALLOC_STRUCT(&ast->nodes_stmts, Ast_Node);
                *STRUCT_AT_POS(ast->nodes_stmts, Ast_Node, pos) = (Ast_Node) {
                    .flags = 0,
                    .depth = 0,
                    .lex_type = TOKEN_OPERATOR,
                    .precedence = 3,
                    .builtin_id = ID_PAREN_CLOSE,
                    .left_node = 0,
                    .right_node = 0,
                    .next_node = 0,
                    .token_start = 0,
                    .token_len = 0
                };
                flags &= ~FLAG_INSERTED_PAREN;
                //printf("Insert close\n");
            }

            if (last_lex_type == TOKEN_IDENTIFIER && last_id != 0 && (id & 0xffff) != ID_BRACE_OPEN) {
                pos = ALLOC_STRUCT(&ast->nodes_stmts, Ast_Node);
                *STRUCT_AT_POS(ast->nodes_stmts, Ast_Node, pos) = (Ast_Node) {
                    .flags = 0,
                    .depth = 0,
                    .lex_type = TOKEN_OPERATOR,
                    .precedence = 3,
                    .builtin_id = ID_PAREN_OPEN,
                    .left_node = 0,
                    .right_node = 0,
                    .next_node = 0,
                    .token_start = 0,
                    .token_len = 0
                };
                flags |= FLAG_INSERTED_PAREN;
                //printf("Insert open\n");
            }
            
            if (old_type == TOKEN_OPERATOR && !is_binary)
                old_type = TOKEN_UNARY;

            pos = ALLOC_STRUCT(&ast->nodes_stmts, Ast_Node);
            *STRUCT_AT_POS(ast->nodes_stmts, Ast_Node, pos) = (Ast_Node) {
                .flags = 0,
                .depth = 0,
                .lex_type = (char)old_type,
                .precedence = (char)((id >> 16) & 0x7f),
                .builtin_id = (short)(id & 0xffff),
                .left_node = 0,
                .right_node = 0,
                .next_node = 0,
                .token_start = start,
                .token_len = len
            };

            if (false) {
                int dbg_len = i - start + inc;
                if (dbg_len > 63) dbg_len = 63;
                memcpy(token_dbg, &buf[start], dbg_len);
                token_dbg[dbg_len] = 0;
                printf("%c: \"%s\"\n", "CSNIO"[old_type-1], token_dbg);
            }

            start = i + inc;
            if (old_type != TOKEN_COMMENT)
                last_lex_type = old_type;

            if (old_type == TOKEN_IDENTIFIER) {
                if (last_identifier_pos > 0) {
                    int last_id = STRUCT_AT_POS(ast->nodes_stmts, Ast_Node, last_identifier_pos)->builtin_id;
                    if (last_id == ID_MODULE) {
                        module = pos;
                    }
                    else if (last_id == ID_FUNC || last_id == ID_MACRO) {
                        int method = ALLOC_STRUCT(&ast->globals, Ast_Global);
                        *STRUCT_AT_POS(ast->globals, Ast_Global, method) = (Ast_Global) {
                            .type_flags = last_id == ID_FUNC ? FLAG_TYPE_FUNC : FLAG_TYPE_MACRO,
                            .name_token = pos,
                            .first_stmt_pos = 0,
                            .n_stmts = 0,
                            .parent_idx = 0,
                            .module_token = module
                        };
                    }
                }

                last_identifier_pos = pos;
            }

            last_id = (short)(id & 0xffff);
        }

        flags &= ~(FLAG_SHOULD_ADD | FLAG_WAS_WS);
        flags |= -(flags & FLAG_IS_WS) & FLAG_WAS_WS;
        old_type = type;
        prev_prev = prev;
        prev = cur;
        i++;
    }

    return 0;
}

int construct_ast(Ast *ast, int n_nodes, Buffer *buffer, IntVector *allocator)
{
    char token_dbg[64];

    int error_index[2];
    int error_lines[2];
    int error_cols[2];

    int brace_levels = 0;

    int cur_name_token = 0;
    int cur_global = 0;
    int n_globals = COUNT_ALLOCD(ast->globals.size, Ast_Global);

    struct {
        int idx;
        int brace;
    } stack_global[16] = {0};
    int depth_stack_global = -1;

    int prev_stmt_connection_type = 0;
    int prev_pos = 0;

    int parent_stack_idx = 0;

    for (int i = 0; i < n_nodes; i++) {
        Ast_Node *node = STRUCT_AT_INDEX(ast->nodes_stmts, Ast_Node, i);
        int end = 0;
        int n_ops = 0;
        while (i + end < n_nodes) {
            short op = node[end].builtin_id;
            n_ops += node[end].precedence > 0;
            end++;

            if (
                op == ID_SEMICOLON || op == ID_COMMA ||
                op == ID_BRACE_OPEN || op == ID_PAREN_OPEN ||
                op == ID_INVOKE_OPEN || op == ID_SQUARE_OPEN || op == ID_TEMPL_OPEN ||
                op == ID_BRACE_CLOSE || op == ID_PAREN_CLOSE ||
                op == ID_SQUARE_CLOSE || op == ID_TEMPL_CLOSE
            )
                break;
        }
        if (end == 0)
            continue;

        if (cur_global < n_globals) {
            int name_token = STRUCT_AT_INDEX(ast->globals, Ast_Global, cur_global)->name_token;
            if (name_token >= i && cur_name_token < i + end) {
                cur_name_token = i;
            }
            if (cur_name_token > 0 && node[end-1].builtin_id == ID_BRACE_OPEN) {
                depth_stack_global++;
                if (depth_stack_global >= 16) {
                    printf("Too many nested functions/macros (exceeds %d)\n", depth_stack_global);
                    return -1;
                }
                stack_global[depth_stack_global].idx = cur_global;
                stack_global[depth_stack_global].brace = brace_levels;

                Ast_Global *g = STRUCT_AT_INDEX(ast->globals, Ast_Global, cur_global);
                g->first_stmt_pos = sizeof(Ast_Statement) + (sizeof(int) * ast->nodes_stmts.size);
                if (depth_stack_global >= 1)
                    g->parent_idx = stack_global[depth_stack_global-1].idx;

                cur_name_token = 0;
                cur_global++;
                brace_levels++;
            }
        }

        int new_size = parent_stack_idx + end * 2;
        if (new_size > allocator->size)
            IntVector_resize(allocator, new_size);

        int *order = &allocator->buf[parent_stack_idx];
        int *stack = &order[end];

        char highest = 0;
        int highest_node = 0;
        for (int j = 0; j < end; j++) {
            if (node[j].precedence > highest) {
                highest = node[j].precedence;
                highest_node = j;
            }
        }

        int depth = 0;
        int ss = 0;
        int cur = 0;
        int idx = highest_node;

        node[highest_node].flags |= FLAG_VISITED;
        do {
            if (ss > 0) {
                idx = stack[--ss];
                depth = node[idx].depth;
            }
            order[cur++] = idx;

            highest = 0;
            int left = -i - 1;
            int left_adj = left;
            int right = -i - 1;
            int right_adj = right;

            int k = idx;
            while (k > 0) {
                k--;
                if ((node[k].flags & FLAG_VISITED) == 0) {
                    if (left_adj < 0)
                        left_adj = k;
                    if (node[k].precedence > highest) {
                        highest = node[k].precedence;
                        left = k;
                    }
                }
            }

            k = idx;
            while (k < end-1) {
                k++;
                if ((node[k].flags & FLAG_VISITED) == 0) {
                    if (right_adj < 0)
                        right_adj = k;
                    if (node[k].precedence > highest) {
                        highest = node[k].precedence;
                        right = k;
                    }
                }
            }

            if (left >= 0)
                stack[ss++] = left;
            else
                left = left_adj;

            if (left >= 0) {
                node[idx].left_node = i + 1 + left;
                node[left].flags |= FLAG_VISITED;
                node[left].depth = depth + 1;
            }

            if (right >= 0)
                stack[ss++] = right;
            else
                right = right_adj;

            if (right >= 0) {
                node[idx].right_node = i + 1 + right;
                node[right].flags |= FLAG_VISITED;
                node[right].depth = depth + 1;
            }

            depth++;

        } while (ss > 0);

        for (int j = cur - 1; j > 0; j--)
            node[order[j]].next_node = i + 1 + order[j-1];

        int first_node = order[cur-1];

        int pos = ALLOC_STRUCT(&ast->nodes_stmts, Ast_Statement);
        *STRUCT_AT_POS(ast->nodes_stmts, Ast_Statement, pos) = (Ast_Statement) {
            .parent = 1,
            .first_node = i + first_node + 1,
            .left_stmt = 0,
            .next_stmt = 0,
            .right_stmt = 0
        };

        int cur_stmt_connection_type = node[end-1].builtin_id;

        switch (prev_stmt_connection_type) {
            case ID_SEMICOLON:
            case ID_COMMA:
            {
                STRUCT_AT_POS(ast->nodes_stmts, Ast_Statement, prev_pos)->next_stmt = pos;
                break;
            }

            case ID_BRACE_OPEN:
            case ID_PAREN_OPEN:
            case ID_INVOKE_OPEN:
            case ID_SQUARE_OPEN:
            case ID_TEMPL_OPEN:
            {
                Ast_Statement *prev = STRUCT_AT_POS(ast->nodes_stmts, Ast_Statement, prev_pos);
                int *leaf = prev_stmt_connection_type == ID_BRACE_OPEN ? &prev->right_stmt : &prev->left_stmt;
                *leaf = pos;

                // Since 'order' is not used for the rest of the loop, and there must be at least 2 (ie. end*2) spare slots at the end of allocator->buf, this is safe.
                allocator->buf[parent_stack_idx++] = i + 1;
                break;
            }

            case ID_BRACE_CLOSE:
            case ID_PAREN_CLOSE:
            case ID_SQUARE_CLOSE:
            case ID_TEMPL_CLOSE:
            {
                break;
            }
        }

        if (parent_stack_idx > 0)
            STRUCT_AT_POS(ast->nodes_stmts, Ast_Statement, pos)->parent = allocator->buf[parent_stack_idx-1];

        const char *ch_close = NULL;
        switch (cur_stmt_connection_type) {
            case ID_BRACE_CLOSE:
                ch_close = "}";
                brace_levels--;
                if (depth_stack_global >= 0 && brace_levels <= stack_global[depth_stack_global].brace) {
                    Ast_Global *g = STRUCT_AT_INDEX(ast->globals, Ast_Global, stack_global[depth_stack_global].idx);
                    g->n_stmts = (g->first_stmt_pos - pos) / sizeof(Ast_Global);
                    depth_stack_global--;
                }
                goto handle_close_token;
            case ID_PAREN_CLOSE:
                ch_close = ")";
                goto handle_close_token;
            case ID_SQUARE_CLOSE:
                ch_close = "]";
                goto handle_close_token;
            case ID_TEMPL_CLOSE:
            {
                ch_close = ">>";
        handle_close_token:

                parent_stack_idx--;
                int parent = 0;
                if (parent_stack_idx >= 0)
                    parent = allocator->buf[parent_stack_idx];

                if (parent <= 0) {
                    resolve_line_column_from_index(buffer, error_index, error_lines, error_cols, 1);
                    printf("Unmatched '%s' at line,col %d,%d\n", ch_close, error_lines[0], error_cols[0]);
                    return -1;
                }

                Ast_Node *parent_node = STRUCT_AT_INDEX(ast->nodes_stmts, Ast_Node, parent-1);

                if (
                    (parent_node->builtin_id == ID_BRACE_OPEN && cur_stmt_connection_type != ID_BRACE_CLOSE) ||
                    ((parent_node->builtin_id == ID_PAREN_OPEN || parent_node->builtin_id == ID_INVOKE_OPEN) && cur_stmt_connection_type != ID_PAREN_CLOSE) ||
                    (parent_node->builtin_id == ID_SQUARE_OPEN && cur_stmt_connection_type != ID_SQUARE_CLOSE) ||
                    (parent_node->builtin_id == ID_TEMPL_OPEN && cur_stmt_connection_type != ID_TEMPL_CLOSE)
                ) {
                    error_index[0] = parent_node->token_start;
                    error_index[1] = STRUCT_AT_INDEX(ast->nodes_stmts, Ast_Node, i)->token_start;
                    resolve_line_column_from_index(buffer, error_index, error_lines, error_cols, 2);
                    printf(
                        "Mismatched brackets for '%s' -> %d at line,col %d,%d to %d,%d\n",
                        ch_close, parent_node->builtin_id, error_lines[0], error_cols[0], error_lines[1], error_cols[1]
                    );
                    return -1;
                }

                break;
            }
        }

        prev_stmt_connection_type = cur_stmt_connection_type;
        prev_pos = pos;

        i += end - 1;
    }

    return 0;
}

int typecheck_prepass(Ast *ast, Buffer *buffer, IntVector *allocator)
{
    int first = ast->first_stmt;
    int n_stmts = ast->n_stmts;

    int module_token = -1;
    int func_token = -1;

    for (int i = 0; i < n_stmts; i++) {
        Ast_Type type = {0};
        
    }
}

int parse_source_file(Ast *ast, Buffer *buffer, int buffer_idx, IntVector *allocator)
{
    *ast = (Ast) {0};
    ast->buffer_idx = buffer_idx;
    if (lex_source(ast, buffer) != 0) {
        return -1;
    }

    int first_statement = ast->nodes_stmts.size;
    int n_nodes = COUNT_ALLOCD(first_statement, Ast_Node);
    ast->first_stmt = first_statement;
    if (construct_ast(ast, n_nodes, buffer, allocator) != 0) {
        return -1;
    }

    int statement_end = ast->nodes_stmts.size;
    int n_statements = COUNT_ALLOCD(statement_end - first_statement, Ast_Statement);
    ast->n_stmts = n_statements;

    return typecheck_prepass(ast, buffer, allocator);
}

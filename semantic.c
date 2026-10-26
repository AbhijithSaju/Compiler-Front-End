#include <stdio.h>
#define MAX_SOURCE 65536
#define MAX_TOKENS 20000
#define MAX_ERRORS 100
typedef enum { TOK_EOF,TOK_ID,TOK_NUMBER,TOK_SWITCH,TOK_CASE,TOK_DEFAULT,TOK_BREAK,TOK_WHILE,TOK_LPAREN,TOK_RPAREN,TOK_LBRACE,TOK_RBRACE,TOK_COLON,TOK_SEMI,TOK_COMMA,TOK_PLUS,TOK_MINUS,TOK_STAR,TOK_SLASH,TOK_PERCENT,TOK_ASSIGN,TOK_EQ,TOK_NE,TOK_LT,TOK_LE,TOK_GT,TOK_GE,TOK_AND,TOK_OR,TOK_NOT,TOK_INVALID } TokenKind;
typedef struct { TokenKind kind; char text[128]; long value; int line,column; } Token;
typedef enum { AST_PROGRAM,AST_SWITCH,AST_CASE,AST_DEFAULT,AST_BREAK,AST_WHILE,AST_BLOCK,AST_EXPR_STMT,AST_EMPTY,AST_IDENTIFIER,AST_INTEGER,AST_UNARY,AST_BINARY,AST_ERROR } AstKind;
typedef struct Ast { AstKind kind; char text[128]; long value; int line,column; struct Ast *child,*next; } Ast;
extern Token tokens[MAX_TOKENS];
extern int token_count,token_pos,error_count,lexical_error_count,syntax_error_count,semantic_error_count;
const char *token_name(TokenKind);
void report_error_at(int,int,const char*,const char*);
void lexical_error(int,int,const char*);
void syntax_error(int,int,const char*);
void semantic_error(int,int,const char*);
void add_token(TokenKind,const char*,long,int,int);
void lex_source(const char*);
Ast *new_ast(AstKind,const char*,long,int,int);
void append_child(Ast*,Ast*);
const char *ast_name(AstKind);
void print_ast(const Ast*,int);
void free_ast(Ast*);
Ast *parse_program(void);
int eval_constant(const Ast*,long*);
void analyze_semantics(const Ast*);
void print_tokens(void);
char *read_source_file(const char*);
char *read_stdin_source(void);

#include <limits.h>
#include <stdio.h>
#include <string.h>
int eval_constant(const Ast *n, long *out) {
    if (!n) return 0;
    if (n->kind == AST_INTEGER) { *out = n->value; return 1; }
    if (n->kind == AST_UNARY) {
        long v;
        if (!eval_constant(n->child, &v)) return 0;
        if (strcmp(n->text, "+") == 0) { *out = v; return 1; }
        if (strcmp(n->text, "-") == 0) {
            if (v == LONG_MIN) return 0;
            *out = -v; return 1;
        }
        if (strcmp(n->text, "!") == 0) { *out = !v; return 1; }
        return 0;
    }
    if (n->kind == AST_BINARY) {
        long a, b;
        if (!eval_constant(n->child, &a) || !eval_constant(n->child->next, &b)) return 0;
        if (strcmp(n->text, "+") == 0) {
            if ((b > 0 && a > LONG_MAX - b) || (b < 0 && a < LONG_MIN - b)) return 0;
            *out = a + b; return 1;
        }
        if (strcmp(n->text, "-") == 0) {
            if ((b < 0 && a > LONG_MAX + b) || (b > 0 && a < LONG_MIN + b)) return 0;
            *out = a - b; return 1;
        }
        if (strcmp(n->text, "*") == 0) {
            if (a != 0 && (a == -1 ? b == LONG_MIN : b > LONG_MAX / a || b < LONG_MIN / a)) return 0;
            *out = a * b; return 1;
        }
        if (strcmp(n->text, "/") == 0) {
            if (b == 0 || (a == LONG_MIN && b == -1)) return 0;
            *out = a / b; return 1;
        }
        if (strcmp(n->text, "%") == 0) {
            if (b == 0 || (a == LONG_MIN && b == -1)) return 0;
            *out = a % b; return 1;
        }
        if (strcmp(n->text, "==") == 0) { *out = (a == b); return 1; }
        if (strcmp(n->text, "!=") == 0) { *out = (a != b); return 1; }
        if (strcmp(n->text, "<") == 0) { *out = (a < b); return 1; }
        if (strcmp(n->text, "<=") == 0) { *out = (a <= b); return 1; }
        if (strcmp(n->text, ">") == 0) { *out = (a > b); return 1; }
        if (strcmp(n->text, ">=") == 0) { *out = (a >= b); return 1; }
        if (strcmp(n->text, "&&") == 0) { *out = (a && b); return 1; }
        if (strcmp(n->text, "||") == 0) { *out = (a || b); return 1; }
    }
    return 0;
}
static void analyze_switch_semantics(const Ast *sw) {
    long seen_values[512];
    int seen_count = 0;
    int default_seen = 0;

    /* The first child is the switch controlling expression. */
    const Ast *part = sw->child ? sw->child->next : NULL;
    for (; part; part = part->next) {
        if (part->kind == AST_CASE) {
            const Ast *case_expr = part->child;
            long value = 0;

            if (!eval_constant(case_expr, &value)) {
                semantic_error(part->line, part->column,
                               "case label must be an integer constant expression");
            } else {
                int duplicate = 0;
                for (int i = 0; i < seen_count; i++) {
                    if (seen_values[i] == value) {
                        duplicate = 1;
                        break;
                    }
                }

                if (duplicate) {
                    char msg[160];
                    snprintf(msg, sizeof(msg), "duplicate case value %ld", value);
                    semantic_error(part->line, part->column, msg);
                } else if (seen_count < (int)(sizeof(seen_values) / sizeof(seen_values[0]))) {
                    seen_values[seen_count++] = value;
                } else {
                    semantic_error(part->line, part->column,
                                   "too many case labels in one switch");
                }
            }
        } else if (part->kind == AST_DEFAULT) {
            if (default_seen) {
                semantic_error(part->line, part->column,
                               "a switch statement may contain only one default label");
            }
            default_seen = 1;
        }
    }
}

static void analyze_semantics_node(const Ast *node, int breakable_depth) {
    for (const Ast *n = node; n; n = n->next) {
        if (n->kind == AST_BREAK && breakable_depth == 0) {
            semantic_error(n->line, n->column,
                           "'break' must appear inside a switch or loop");
        }

        if (n->kind == AST_SWITCH) {
            analyze_switch_semantics(n);
            /* A switch makes break legal in its case/default bodies. */
            analyze_semantics_node(n->child, breakable_depth + 1);
        } else if (n->kind == AST_WHILE) {
            /* The condition is not a breakable body; the statement body is. */
            if (n->child) {
                analyze_semantics_node(n->child, breakable_depth);
                if (n->child->next)
                    analyze_semantics_node(n->child->next, breakable_depth + 1);
            }
        } else {
            analyze_semantics_node(n->child, breakable_depth);
        }
    }
}

void analyze_semantics(const Ast *root) {
    analyze_semantics_node(root, 0);
}


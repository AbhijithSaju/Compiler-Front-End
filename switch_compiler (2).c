/*
 * Refined Switch Statement Compiler Front End
 * -------------------------------------------
 * Features:
 *   1. Lexer: tokenizes a small C-like language.
 *   2. Parser: parses switch/case/default/break and basic expressions.
 *   3. Semantic checks: duplicate constant case labels, duplicate default,
 *      and break outside switch/loop.
 *   4. Dynamic AST construction and readable AST printing.
 *
 * Build:
 *   gcc -std=c11 -Wall -Wextra -pedantic switch_compiler.c -o switch_compiler.exe
 *
 * Run:
 *   switch_compiler.exe
 *   or: switch_compiler.exe input.txt
 *
 * This is an educational compiler front end for a restricted C-like subset,
 * not a complete ISO C compiler.
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SOURCE 65536
#define MAX_TOKENS 20000
#define MAX_ERRORS 100

typedef enum {
    TOK_EOF, TOK_ID, TOK_NUMBER,
    TOK_SWITCH, TOK_CASE, TOK_DEFAULT, TOK_BREAK,
    TOK_WHILE,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_COLON, TOK_SEMI, TOK_COMMA,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    TOK_ASSIGN, TOK_EQ, TOK_NE, TOK_LT, TOK_LE, TOK_GT, TOK_GE,
    TOK_AND, TOK_OR, TOK_NOT,
    TOK_INVALID
} TokenKind;

typedef struct {
    TokenKind kind;
    char text[128];
    long value;
    int line;
    int column;
} Token;

static Token tokens[MAX_TOKENS];
static int token_count = 0;
static int token_pos = 0;
static int error_count = 0;

static const char *token_name(TokenKind k) {
    switch (k) {
        case TOK_EOF: return "EOF";
        case TOK_ID: return "IDENTIFIER";
        case TOK_NUMBER: return "INTEGER";
        case TOK_SWITCH: return "switch";
        case TOK_CASE: return "case";
        case TOK_DEFAULT: return "default";
        case TOK_BREAK: return "break";
        case TOK_WHILE: return "while";
        case TOK_LPAREN: return "(";
        case TOK_RPAREN: return ")";
        case TOK_LBRACE: return "{";
        case TOK_RBRACE: return "}";
        case TOK_COLON: return ":";
        case TOK_SEMI: return ";";
        case TOK_COMMA: return ",";
        case TOK_PLUS: return "+";
        case TOK_MINUS: return "-";
        case TOK_STAR: return "*";
        case TOK_SLASH: return "/";
        case TOK_PERCENT: return "%";
        case TOK_ASSIGN: return "=";
        case TOK_EQ: return "==";
        case TOK_NE: return "!=";
        case TOK_LT: return "<";
        case TOK_LE: return "<=";
        case TOK_GT: return ">";
        case TOK_GE: return ">=";
        case TOK_AND: return "&&";
        case TOK_OR: return "||";
        case TOK_NOT: return "!";
        case TOK_INVALID: return "INVALID";
    }
    return "UNKNOWN";
}

static void report_error_at(int line, int column, const char *kind,
                            const char *message) {
    if (error_count < MAX_ERRORS) {
        fprintf(stderr, "[%s] line %d, column %d: %s\n",
                kind, line, column, message);
    }
    error_count++;
}

static void lexical_error(int line, int column, const char *message) {
    report_error_at(line, column, "LEXICAL ERROR", message);
}

static void syntax_error(int line, int column, const char *message) {
    report_error_at(line, column, "SYNTAX ERROR", message);
}

static void semantic_error(int line, int column, const char *message) {
    report_error_at(line, column, "SEMANTIC ERROR", message);
}

static void add_token(TokenKind kind, const char *text, long value,
                      int line, int column) {
    if (token_count >= MAX_TOKENS - 1) {
        lexical_error(line, column, "token limit exceeded");
        return;
    }
    Token *t = &tokens[token_count++];
    t->kind = kind;
    snprintf(t->text, sizeof(t->text), "%s", text);
    t->value = value;
    t->line = line;
    t->column = column;
}

static int is_id_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static int is_id_part(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static TokenKind keyword_kind(const char *s) {
    if (strcmp(s, "switch") == 0) return TOK_SWITCH;
    if (strcmp(s, "case") == 0) return TOK_CASE;
    if (strcmp(s, "default") == 0) return TOK_DEFAULT;
    if (strcmp(s, "break") == 0) return TOK_BREAK;
    if (strcmp(s, "while") == 0) return TOK_WHILE;
    return TOK_ID;
}

static void lex_source(const char *src) {
    int i = 0, line = 1, col = 1;
    token_count = 0;
    while (src[i] != '\0') {
        char c = src[i];

        if (isspace((unsigned char)c)) {
            if (c == '\n') { line++; col = 1; }
            else col++;
            i++;
            continue;
        }

        if (c == '/' && src[i + 1] == '/') {
            while (src[i] && src[i] != '\n') { i++; col++; }
            continue;
        }
        if (c == '/' && src[i + 1] == '*') {
            int start_line = line, start_col = col;
            i += 2; col += 2;
            int closed = 0;
            while (src[i]) {
                if (src[i] == '*' && src[i + 1] == '/') {
                    i += 2; col += 2; closed = 1; break;
                }
                if (src[i] == '\n') { line++; col = 1; i++; }
                else { col++; i++; }
            }
            if (!closed) lexical_error(start_line, start_col, "unterminated block comment");
            continue;
        }

        int start_line = line, start_col = col;
        if (is_id_start(c)) {
            char buf[128]; int n = 0;
            while (is_id_part(src[i])) {
                if (n < (int)sizeof(buf) - 1) buf[n++] = src[i];
                i++; col++;
            }
            buf[n] = '\0';
            add_token(keyword_kind(buf), buf, 0, start_line, start_col);
            continue;
        }

        if (isdigit((unsigned char)c)) {
            char buf[128]; int n = 0;
            while (isalnum((unsigned char)src[i]) || src[i] == '_') {
                if (n < (int)sizeof(buf) - 1) buf[n++] = src[i];
                i++; col++;
            }
            buf[n] = '\0';
            errno = 0;
            char *end = NULL;
            long val = strtol(buf, &end, 0);
            if (errno == ERANGE || end == buf || *end != '\0') {
                lexical_error(start_line, start_col, "invalid or out-of-range integer literal");
                add_token(TOK_INVALID, buf, 0, start_line, start_col);
            } else {
                add_token(TOK_NUMBER, buf, val, start_line, start_col);
            }
            continue;
        }

        TokenKind k = TOK_INVALID;
        int consumed = 1;
        switch (c) {
            case '(': k = TOK_LPAREN; break;
            case ')': k = TOK_RPAREN; break;
            case '{': k = TOK_LBRACE; break;
            case '}': k = TOK_RBRACE; break;
            case ':': k = TOK_COLON; break;
            case ';': k = TOK_SEMI; break;
            case ',': k = TOK_COMMA; break;
            case '+': k = TOK_PLUS; break;
            case '-': k = TOK_MINUS; break;
            case '*': k = TOK_STAR; break;
            case '%': k = TOK_PERCENT; break;
            case '!':
                if (src[i + 1] == '=') { k = TOK_NE; consumed = 2; }
                else k = TOK_NOT;
                break;
            case '=':
                if (src[i + 1] == '=') { k = TOK_EQ; consumed = 2; }
                else k = TOK_ASSIGN;
                break;
            case '<':
                if (src[i + 1] == '=') { k = TOK_LE; consumed = 2; }
                else k = TOK_LT;
                break;
            case '>':
                if (src[i + 1] == '=') { k = TOK_GE; consumed = 2; }
                else k = TOK_GT;
                break;
            case '&':
                if (src[i + 1] == '&') { k = TOK_AND; consumed = 2; }
                break;
            case '|':
                if (src[i + 1] == '|') { k = TOK_OR; consumed = 2; }
                break;
            case '/':
                k = TOK_SLASH;
                break;
            default:
                break;
        }

        if (k == TOK_INVALID) {
            char msg[160];
            snprintf(msg, sizeof(msg), "unexpected character '%c'", c);
            lexical_error(line, col, msg);
            add_token(TOK_INVALID, (char[]){c, '\0'}, 0, line, col);
            i++; col++;
        } else {
            char spelling[3] = {src[i], '\0', '\0'};
            if (consumed == 2) spelling[1] = src[i + 1];
            add_token(k, spelling, 0, start_line, start_col);
            i += consumed; col += consumed;
        }
    }
    add_token(TOK_EOF, "<EOF>", 0, line, col);
}

/* ------------------------------- AST ----------------------------------- */

typedef enum {
    AST_PROGRAM, AST_SWITCH, AST_CASE, AST_DEFAULT, AST_BREAK, AST_WHILE,
    AST_BLOCK, AST_EXPR_STMT, AST_EMPTY, AST_IDENTIFIER, AST_INTEGER,
    AST_UNARY, AST_BINARY, AST_ERROR
} AstKind;

typedef struct Ast {
    AstKind kind;
    char text[128];
    long value;
    int line, column;
    struct Ast *child;
    struct Ast *next;
} Ast;

static Ast *new_ast(AstKind kind, const char *text, long value, int line, int col) {
    Ast *n = (Ast *)calloc(1, sizeof(Ast));
    if (!n) {
        fprintf(stderr, "Fatal error: not enough memory for AST.\n");
        exit(EXIT_FAILURE);
    }
    n->kind = kind;
    if (text) snprintf(n->text, sizeof(n->text), "%s", text);
    n->value = value;
    n->line = line;
    n->column = col;
    return n;
}

static void append_child(Ast *parent, Ast *child) {
    if (!parent || !child) return;
    if (!parent->child) parent->child = child;
    else {
        Ast *p = parent->child;
        while (p->next) p = p->next;
        p->next = child;
    }
}

static const char *ast_name(AstKind k) {
    switch (k) {
        case AST_PROGRAM: return "Program";
        case AST_SWITCH: return "Switch";
        case AST_CASE: return "Case";
        case AST_DEFAULT: return "Default";
        case AST_BREAK: return "Break";
        case AST_WHILE: return "While";
        case AST_BLOCK: return "Block";
        case AST_EXPR_STMT: return "ExpressionStatement";
        case AST_EMPTY: return "EmptyStatement";
        case AST_IDENTIFIER: return "Identifier";
        case AST_INTEGER: return "Integer";
        case AST_UNARY: return "UnaryExpression";
        case AST_BINARY: return "BinaryExpression";
        case AST_ERROR: return "Error";
    }
    return "Unknown";
}

static void print_ast(const Ast *n, int depth) {
    while (n) {
        for (int i = 0; i < depth; i++) printf("  ");
        printf("%s", ast_name(n->kind));
        if (n->text[0]) printf(" [%s]", n->text);
        if (n->kind == AST_INTEGER) printf(" = %ld", n->value);
        printf("  (line %d)\n", n->line);
        if (n->child) print_ast(n->child, depth + 1);
        n = n->next;
    }
}

static void free_ast(Ast *n) {
    while (n) {
        Ast *next = n->next;
        free_ast(n->child);
        free(n);
        n = next;
    }
}

/* ------------------------------ Parser --------------------------------- */

static Token *current(void) { return &tokens[token_pos]; }
static int match(TokenKind k) {
    if (current()->kind == k) { if (k != TOK_EOF) token_pos++; return 1; }
    return 0;
}
static Token *expect(TokenKind k, const char *message) {
    if (current()->kind == k) {
        Token *t = current();
        if (k != TOK_EOF) token_pos++;
        return t;
    }
    syntax_error(current()->line, current()->column, message);
    return NULL;
}

static Ast *parse_statement(void);
static Ast *parse_expression(void);

static Ast *parse_primary(void) {
    Token *t = current();
    if (match(TOK_NUMBER)) return new_ast(AST_INTEGER, t->text, t->value, t->line, t->column);
    if (match(TOK_ID)) return new_ast(AST_IDENTIFIER, t->text, 0, t->line, t->column);
    if (match(TOK_LPAREN)) {
        Ast *e = parse_expression();
        expect(TOK_RPAREN, "expected ')' after expression");
        return e;
    }
    syntax_error(t->line, t->column, "expected an identifier, integer, or parenthesized expression");
    if (t->kind != TOK_EOF && t->kind != TOK_RBRACE && t->kind != TOK_SEMI &&
        t->kind != TOK_COLON) token_pos++;
    return new_ast(AST_ERROR, "invalid expression", 0, t->line, t->column);
}

static Ast *parse_unary(void) {
    Token *t = current();
    if (match(TOK_PLUS) || match(TOK_MINUS) || match(TOK_NOT)) {
        Ast *n = new_ast(AST_UNARY, t->text, 0, t->line, t->column);
        append_child(n, parse_unary());
        return n;
    }
    return parse_primary();
}

static int precedence(TokenKind k) {
    switch (k) {
        case TOK_OR: return 1;
        case TOK_AND: return 2;
        case TOK_EQ: case TOK_NE: return 3;
        case TOK_LT: case TOK_LE: case TOK_GT: case TOK_GE: return 4;
        case TOK_PLUS: case TOK_MINUS: return 5;
        case TOK_STAR: case TOK_SLASH: case TOK_PERCENT: return 6;
        default: return 0;
    }
}

static Ast *parse_binary_rhs(int min_prec, Ast *left) {
    while (precedence(current()->kind) >= min_prec) {
        Token *op = current();
        int prec = precedence(op->kind);
        token_pos++;
        Ast *right = parse_unary();
        while (precedence(current()->kind) > prec)
            right = parse_binary_rhs(prec + 1, right);
        Ast *parent = new_ast(AST_BINARY, op->text, 0, op->line, op->column);
        append_child(parent, left);
        append_child(parent, right);
        left = parent;
    }
    return left;
}

static Ast *parse_expression(void) {
    Ast *left = parse_unary();
    left = parse_binary_rhs(1, left);

    /* Assignment is right-associative and has lower precedence than other operators. */
    if (current()->kind == TOK_ASSIGN) {
        Token *op = current();
        token_pos++;
        Ast *right = parse_expression();
        Ast *assignment = new_ast(AST_BINARY, op->text, 0, op->line, op->column);
        append_child(assignment, left);
        append_child(assignment, right);
        return assignment;
    }
    return left;
}

static int eval_constant(const Ast *n, long *out) {
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

static int inside_breakable = 0;

static Ast *parse_block(void) {
    Token *open = expect(TOK_LBRACE, "expected '{' to begin block");
    Ast *block = new_ast(AST_BLOCK, "{}", 0, open ? open->line : current()->line,
                         open ? open->column : current()->column);
    while (current()->kind != TOK_RBRACE && current()->kind != TOK_EOF) {
        int before = token_pos;
        append_child(block, parse_statement());
        if (token_pos == before) token_pos++;
    }
    expect(TOK_RBRACE, "expected '}' to close block");
    return block;
}

static Ast *parse_switch(void) {
    Token *kw = current(); token_pos++;
    Ast *sw = new_ast(AST_SWITCH, "switch", 0, kw->line, kw->column);
    expect(TOK_LPAREN, "expected '(' after switch");
    append_child(sw, parse_expression());
    expect(TOK_RPAREN, "expected ')' after switch expression");
    expect(TOK_LBRACE, "expected '{' before switch body");

    long seen_values[512];
    int seen_count = 0, default_seen = 0;
    int old_breakable = inside_breakable;
    inside_breakable = 1;

    while (current()->kind != TOK_RBRACE && current()->kind != TOK_EOF) {
        int before = token_pos;
        if (match(TOK_CASE)) {
            Token *label = &tokens[token_pos - 1];
            Ast *c = new_ast(AST_CASE, "case", 0, label->line, label->column);
            Ast *expr = parse_expression();
            append_child(c, expr);
            expect(TOK_COLON, "expected ':' after case label");

            long value;
            if (!eval_constant(expr, &value)) {
                semantic_error(label->line, label->column,
                               "case label must be an integer constant expression");
            } else {
                int duplicate = 0;
                for (int i = 0; i < seen_count; i++)
                    if (seen_values[i] == value) duplicate = 1;
                if (duplicate) {
                    char msg[160];
                    snprintf(msg, sizeof(msg), "duplicate case value %ld", value);
                    semantic_error(label->line, label->column, msg);
                } else if (seen_count < (int)(sizeof(seen_values) / sizeof(seen_values[0]))) {
                    seen_values[seen_count++] = value;
                } else {
                    semantic_error(label->line, label->column, "too many case labels in one switch");
                }
            }

            while (current()->kind != TOK_CASE && current()->kind != TOK_DEFAULT &&
                   current()->kind != TOK_RBRACE && current()->kind != TOK_EOF) {
                int body_before = token_pos;
                append_child(c, parse_statement());
                if (token_pos == body_before) token_pos++;
            }
            append_child(sw, c);
        } else if (match(TOK_DEFAULT)) {
            Token *label = &tokens[token_pos - 1];
            Ast *d = new_ast(AST_DEFAULT, "default", 0, label->line, label->column);
            if (default_seen) semantic_error(label->line, label->column,
                                             "a switch statement may contain only one default label");
            default_seen = 1;
            expect(TOK_COLON, "expected ':' after default");
            while (current()->kind != TOK_CASE && current()->kind != TOK_DEFAULT &&
                   current()->kind != TOK_RBRACE && current()->kind != TOK_EOF) {
                int body_before = token_pos;
                append_child(d, parse_statement());
                if (token_pos == body_before) token_pos++;
            }
            append_child(sw, d);
        } else {
            syntax_error(current()->line, current()->column,
                         "expected 'case', 'default', or '}' in switch body");
            token_pos++;
        }
        if (token_pos == before) token_pos++;
    }
    expect(TOK_RBRACE, "expected '}' after switch body");
    inside_breakable = old_breakable;
    return sw;
}

static Ast *parse_statement(void) {
    Token *t = current();
    if (t->kind == TOK_SWITCH) return parse_switch();

    if (match(TOK_BREAK)) {
        Ast *n = new_ast(AST_BREAK, "break", 0, t->line, t->column);
        if (!inside_breakable) semantic_error(t->line, t->column,
                                              "'break' must appear inside a switch or loop");
        expect(TOK_SEMI, "expected ';' after break");
        return n;
    }

    if (match(TOK_LBRACE)) {
        token_pos--;
        return parse_block();
    }

    if (match(TOK_WHILE)) {
        Ast *n = new_ast(AST_WHILE, "while", 0, t->line, t->column);
        expect(TOK_LPAREN, "expected '(' after while");
        append_child(n, parse_expression());
        expect(TOK_RPAREN, "expected ')' after while condition");
        int old_breakable = inside_breakable;
        inside_breakable = 1;
        append_child(n, parse_statement());
        inside_breakable = old_breakable;
        return n;
    }

    if (match(TOK_SEMI)) return new_ast(AST_EMPTY, ";", 0, t->line, t->column);

    if (t->kind == TOK_CASE || t->kind == TOK_DEFAULT) {
        syntax_error(t->line, t->column, "case/default label is only valid directly in a switch body");
        token_pos++;
        return new_ast(AST_ERROR, "misplaced label", 0, t->line, t->column);
    }

    Ast *expr = parse_expression();
    expect(TOK_SEMI, "expected ';' after expression");
    Ast *stmt = new_ast(AST_EXPR_STMT, "expression", 0, t->line, t->column);
    append_child(stmt, expr);
    return stmt;
}

static Ast *parse_program(void) {
    Ast *root = new_ast(AST_PROGRAM, "source", 0, 1, 1);
    while (current()->kind != TOK_EOF) {
        int before = token_pos;
        append_child(root, parse_statement());
        if (token_pos == before) token_pos++;
    }
    return root;
}

static void print_tokens(void) {
    printf("\n========== TOKEN STREAM ==========\n");
    printf("%-5s %-18s %-18s %s\n", "No.", "Token", "Lexeme", "Location");
    printf("-----------------------------------------------------------\n");
    for (int i = 0; i < token_count; i++) {
        printf("%-5d %-18s %-18s %d:%d\n", i + 1,
               token_name(tokens[i].kind), tokens[i].text,
               tokens[i].line, tokens[i].column);
    }
}

static char *read_source_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open file: %s\n", path);
        return NULL;
    }
    char *buf = (char *)malloc(MAX_SOURCE);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, MAX_SOURCE - 1, f);
    if (ferror(f)) {
        fprintf(stderr, "Error reading file: %s\n", path);
        free(buf); fclose(f); return NULL;
    }
    buf[n] = '\0';
    if (n == MAX_SOURCE - 1) {
        fprintf(stderr, "Warning: input may be truncated at %d bytes.\n", MAX_SOURCE - 1);
    }
    fclose(f);
    return buf;
}

static char *read_stdin_source(void) {
    char *buf = (char *)malloc(MAX_SOURCE);
    if (!buf) return NULL;
    size_t used = 0;
    int c;
    while ((c = getchar()) != EOF) {
        if (used >= MAX_SOURCE - 1) {
            fprintf(stderr, "Input limit reached (%d bytes); remaining input ignored.\n",
                    MAX_SOURCE - 1);
            break;
        }
        buf[used++] = (char)c;
    }
    buf[used] = '\0';
    return buf;
}

int main(int argc, char **argv) {
    printf("============================================\n");
    printf("     SWITCH STATEMENT COMPILER FRONT END\n");
    printf("============================================\n");

    char *source = NULL;
    if (argc > 1) {
        source = read_source_file(argv[1]);
        if (!source) return EXIT_FAILURE;
    } else {
        printf("Enter source code below.\n");
        printf("Finish with Ctrl+Z then Enter on Windows, or Ctrl+D on Linux/macOS.\n\n");
        source = read_stdin_source();
        if (!source) {
            fprintf(stderr, "Unable to allocate input buffer.\n");
            return EXIT_FAILURE;
        }
    }

    error_count = 0;
    printf("========== COMPILER PIPELINE ==========\n");
    printf("[1/4] Lexical analysis...\n");
    lex_source(source);
    print_tokens();

    printf("\n[2/4] Syntax analysis and AST construction...\n");
    token_pos = 0;
    Ast *root = parse_program();

    printf("\n========== ABSTRACT SYNTAX TREE ==========\n");
    print_ast(root, 0);

    printf("\n========== COMPILATION SUMMARY ==========\n");
    if (error_count == 0) {
        printf("Result: SUCCESS\n");
        printf("Lexical analysis: passed\n");
        printf("Syntax analysis: passed\n");
        printf("Semantic validation: passed\n");
        printf("AST generation: completed\n");
    } else {
        printf("Result: FAILED\n");
        printf("Total errors: %d\n", error_count);
        printf("Review the diagnostic messages above.\n");
    }
    printf("\nNote: this educational front end supports a restricted C-like subset.\n");

    free_ast(root);
    free(source);
    return error_count == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

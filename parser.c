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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    Token *kw = current();
    token_pos++;
    Ast *sw = new_ast(AST_SWITCH, "switch", 0, kw->line, kw->column);

    expect(TOK_LPAREN, "expected '(' after switch");
    append_child(sw, parse_expression());
    expect(TOK_RPAREN, "expected ')' after switch expression");
    expect(TOK_LBRACE, "expected '{' before switch body");

    while (current()->kind != TOK_RBRACE && current()->kind != TOK_EOF) {
        int before = token_pos;

        if (match(TOK_CASE)) {
            Token *label = &tokens[token_pos - 1];
            Ast *c = new_ast(AST_CASE, "case", 0, label->line, label->column);
            append_child(c, parse_expression());
            expect(TOK_COLON, "expected ':' after case label");

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
    return sw;
}

static Ast *parse_statement(void) {
    Token *t = current();
    if (t->kind == TOK_SWITCH) return parse_switch();

    if (match(TOK_BREAK)) {
        Ast *n = new_ast(AST_BREAK, "break", 0, t->line, t->column);
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
        append_child(n, parse_statement());
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

Ast *parse_program(void) {
    Ast *root = new_ast(AST_PROGRAM, "source", 0, 1, 1);
    while (current()->kind != TOK_EOF) {
        int before = token_pos;
        append_child(root, parse_statement());
        if (token_pos == before) token_pos++;
    }
    return root;
}


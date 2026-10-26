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
Ast *new_ast(AstKind kind, const char *text, long value, int line, int col) {
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

void append_child(Ast *parent, Ast *child) {
    if (!parent || !child) return;
    if (!parent->child) parent->child = child;
    else {
        Ast *p = parent->child;
        while (p->next) p = p->next;
        p->next = child;
    }
}

const char *ast_name(AstKind k) {
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

void print_ast(const Ast *n, int depth) {
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

void free_ast(Ast *n) {
    while (n) {
        Ast *next = n->next;
        free_ast(n->child);
        free(n);
        n = next;
    }
}


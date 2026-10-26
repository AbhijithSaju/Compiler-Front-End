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
void print_tokens(void) {
    printf("\n========== TOKEN STREAM ==========\n");
    printf("%-5s %-18s %-18s %s\n", "No.", "Token", "Lexeme", "Location");
    printf("-----------------------------------------------------------\n");
    for (int i = 0; i < token_count; i++) {
        printf("%-5d %-18s %-18s %d:%d\n", i + 1,
               token_name(tokens[i].kind), tokens[i].text,
               tokens[i].line, tokens[i].column);
    }
}

char *read_source_file(const char *path) {
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

char *read_stdin_source(void) {
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


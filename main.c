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
int main(int argc, char **argv) {
    printf("============================================\n");
    printf("     SWITCH STATEMENT COMPILER FRONT END\n");
    printf("============================================\n");

    char *source = NULL;
    if (argc > 1) {
        source = read_source_file(argv[1]);
        if (!source) return EXIT_FAILURE;
    } else {
        printf("");
        printf("");
        source = read_stdin_source();
        if (!source) {
            fprintf(stderr, "Unable to allocate input buffer.\n");
            return EXIT_FAILURE;
        }
    }

    error_count = lexical_error_count = syntax_error_count = semantic_error_count = 0;
    printf("========== COMPILER PIPELINE ==========\n");
    printf("[1/4] Lexical analysis...\n");
    lex_source(source);
    print_tokens();

    printf("\n[2/4] Syntax analysis and AST construction...\n");
    token_pos = 0;
    Ast *root = parse_program();

    if (lexical_error_count == 0 && syntax_error_count == 0) {
        printf("\n[3/4] Semantic analysis...\n");
        analyze_semantics(root);
        printf("\n========== ABSTRACT SYNTAX TREE ==========\n");
        print_ast(root, 0);
    } else {
        printf("\nSemantic analysis skipped because lexical or syntax errors were found.\n");
        printf("AST display skipped because the source did not pass lexical and syntax analysis.\n");
    }

    printf("\n========== COMPILATION SUMMARY ==========\n");
    if (error_count == 0) {
        printf("Result: SUCCESS\n");
        printf("Lexical analysis: passed\n");
        printf("Syntax analysis: passed\n");
        printf("Semantic validation: passed\n");
        printf("AST generation: completed and displayed\n");
    } else {
        printf("Result: FAILED\n");
        printf("Total errors: %d\n", error_count);
        printf("Lexical errors: %d\n", lexical_error_count);
        printf("Syntax errors: %d\n", syntax_error_count);
        printf("Semantic errors: %d\n", semantic_error_count);
        if (lexical_error_count == 0 && syntax_error_count == 0)
            printf("AST generation: completed; semantic validation failed\n");
        else
            printf("AST generation: partial internal tree discarded from output\n");
        printf("Review the diagnostic messages above.\n");
    }

    free_ast(root);
    free(source);
    return error_count == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

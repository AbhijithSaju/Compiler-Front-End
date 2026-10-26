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

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
Token tokens[MAX_TOKENS]; int token_count=0, token_pos=0;
int error_count=0, lexical_error_count=0, syntax_error_count=0, semantic_error_count=0;
const char *token_name(TokenKind k) {
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

void report_error_at(int line, int column, const char *kind,
                            const char *message) {
    if (error_count < MAX_ERRORS) {
        fprintf(stderr, "[%s] line %d, column %d: %s\n",
                kind, line, column, message);
    }
    error_count++;
    if (strcmp(kind, "LEXICAL ERROR") == 0) lexical_error_count++;
    else if (strcmp(kind, "SYNTAX ERROR") == 0) syntax_error_count++;
    else if (strcmp(kind, "SEMANTIC ERROR") == 0) semantic_error_count++;
}

void lexical_error(int line, int column, const char *message) {
    report_error_at(line, column, "LEXICAL ERROR", message);
}

void syntax_error(int line, int column, const char *message) {
    report_error_at(line, column, "SYNTAX ERROR", message);
}

void semantic_error(int line, int column, const char *message) {
    report_error_at(line, column, "SEMANTIC ERROR", message);
}

void add_token(TokenKind kind, const char *text, long value,
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

void lex_source(const char *src) {
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


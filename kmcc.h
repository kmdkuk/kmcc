#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *duplicate(char *str, size_t len);
typedef struct Type Type;
typedef struct Member Member;

//
// tokenize.c
//

typedef enum {
  TK_RESERVED,  // 記号
  TK_IDENT,     // 識別子
  TK_STR,       // 文字列トークン
  TK_NUM,       // 整数トークン
  TK_EOF,       // 入力の終わりを示すトークン
} TokenKind;

typedef struct Token Token;
struct Token {
  TokenKind kind;  // トークンの型
  Token *next;     // 次の入力トークン
  int val;         // kindがTK_NUMの場合，その数値
  char *str;       // トークンの文字列
  int len;         // トークンの長さ

  char *contents;  // '\0'を含む文字列の内容
  char cont_len;   // 文字列の長さ
};

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
void error_tok(Token *tok, char *fmt, ...);
Token *peek(char *s);
Token *consume(char *op);
Token *consume_ident();
void expect(char *op);
int expect_number();
char *expect_ident();
bool at_eof();
Token *new_token(TokenKind kind,
                 Token *cur,
                 char *str,
                 int len);
Token *tokenize();

extern char *filename;
extern char *user_input;
extern Token *token;

//
// parse.c
//

// 変数の型
typedef struct Var Var;
struct Var {
  char *name;  // 変数の名前
  Type *ty;
  bool is_local;

  // ローカル変数
  int offset;  // RBPからのオフセット

  // グローバル変数
  char *contents;
  int cont_len;
};

typedef struct VarList VarList;
struct VarList {
  VarList *next;
  Var *var;
};

// 抽象構文木のノードの種類
typedef enum {
  ND_ADD,        // num + num
  ND_PTR_ADD,    // ptr + num or num +ptr
  ND_SUB,        // num - num
  ND_PTR_SUB,    // ptr - num
  ND_PTR_DIFF,   // ptr - ptr
  ND_MUL,        // *
  ND_DIV,        // /
  ND_EQ,         // ==
  ND_NE,         // !=
  ND_LT,         // <
  ND_LE,         // <=
  ND_ASSIGN,     // =
  ND_MEMBER,     // . (struct member access)
  ND_ADDR,       // &
  ND_DEREF,      // *
  ND_RETURN,     // "return"
  ND_IF,         // "if"
  ND_WHILE,      // "while"
  ND_FOR,        // "for"
  ND_BLOCK,      // { ... }
  ND_FUNC_CALL,  // 関数呼び出し
  ND_EXPR_STMT,  // Expression statement
  ND_STMT_EXPR,  // Statement expression
  ND_VAR,        // ローカル変数
  ND_NUM,        // 整数
  ND_NULL,       // 空の式
} NodeKind;

// 抽象構文木のノードの型
typedef struct Node Node;
struct Node {
  NodeKind kind;  // ノードの型
  Node *next;     // Next node
  Type *ty;
  Token *tok;  // Representative token

  Node *lhs;  // 左辺
  Node *rhs;  // 右辺

  // "if", "while" or "for" statement
  Node *cond;
  Node *then;
  Node *els;
  Node *init;
  Node *inc;

  // Block or statement expression
  Node *body;

  // Struct member access
  Member *member;

  // func
  char *func_name;
  Node *args;

  Var *var;  // kindがND_VARの場合のみ使う．
  int val;   // kindがND_NUMの場合のみ使う
};

typedef struct Function Function;
struct Function {
  Function *next;
  char *name;
  VarList *params;

  Node *node;
  VarList *locals;
  int stack_size;
};

typedef struct Program Program;
struct Program {
  VarList *globals;
  Function *fns;
};

Program *program();

//
// typing.c
//

typedef enum {
  TY_CHAR,
  TY_INT,
  TY_PTR,
  TY_ARRAY,
  TY_STRUCT,
} TypeKind;

struct Type {
  TypeKind kind;
  int size;  // sizeof() value
  int align;
  Type *base;
  int array_len;
  Member *members;  // struct
};

// Struct member
struct Member {
  Member *next;
  Type *ty;
  char *name;
  int offset;
};

extern Type *char_type;
extern Type *int_type;

bool is_integer(Type *ty);
int align_to(int n, int align);
Type *pointer_to(Type *base);
Type *array_of(Type *base, int size);
void add_type(Node *node);

//
// codegen.c
//
void codegen(Program *prog);

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *duplicate(char *str, size_t len);

//
// tokenize.c
//

typedef enum {
  TK_RESERVED,  // 記号
  TK_IDENT,     // 識別子
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
};

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
void error_tok(Token *tok, char *fmt, ...);
Token *consume(char *op);
Token *consume_ident();
void expect(char *op);
int expect_number();
char *expect_ident();
bool at_eof();
Token *new_token(TokenKind kind, Token *cur, char *str, int len);
Token *tokenize();

extern char *user_input;
extern Token *token;

//
// parse.c
//

// ローカル変数の型
typedef struct Var Var;
struct Var {
  char *name;  // 変数の名前
  int len;     // 名前の長さ
  int offset;  // RBPからのオフセット
};

typedef struct VarList VarList;
struct VarList {
  VarList *next;
  Var *var;
};

// 抽象構文木のノードの種類
typedef enum {
  ND_ADD,        // +
  ND_SUB,        // -
  ND_MUL,        // *
  ND_DIV,        // /
  ND_EQ,         // ==
  ND_NE,         // !=
  ND_LT,         // <
  ND_LE,         // <=
  ND_ASSIGN,     // =
  ND_ADDR,       // &
  ND_DEREF,      // *
  ND_RETURN,     // "return"
  ND_IF,         // "if"
  ND_WHILE,      // "while"
  ND_FOR,        // "for"
  ND_BLOCK,      // { ... }
  ND_FUNC_CALL,  // 関数呼び出し
  ND_EXPR_STMT,  // Expression statement
  ND_VAR,        // ローカル変数
  ND_NUM,        // 整数
} NodeKind;

// 抽象構文木のノードの型
typedef struct Node Node;
struct Node {
  NodeKind kind;  // ノードの型
  Node *next;     // Next node
  Token *tok;     // Representative token

  Node *lhs;  // 左辺
  Node *rhs;  // 右辺

  // "if", "while" or "for" statement
  Node *cond;
  Node *then;
  Node *els;
  Node *init;
  Node *inc;

  // Block
  Node *body;

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

Function *program();

//
// codegen.c
//
void codegen(Function *prog);

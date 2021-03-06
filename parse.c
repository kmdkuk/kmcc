#include "kmcc.h"

// ローカル変数，グローバル変数，typedefのためのスコープ
typedef struct VarScope VarScope;
struct VarScope {
  VarScope *next;
  char *name;
  Var *var;
  Type *type_def;
};

// struct tagsのためのスコープ
typedef struct TagScope TagScope;
struct TagScope {
  TagScope *next;
  char *name;
  Type *ty;
};

typedef struct {
  VarScope *var_scope;
  TagScope *tag_scope;
} Scope;

// すべてのローカル変数
static VarList *locals;

// グローバル変数のリスト
static VarList *globals;

// Cには2つのブロックスコープがある．
// 変数/typedefと，構造体のタグ
// 以下2つの変数は，現在のスコープ情報を保持
static VarScope *var_scope;
static TagScope *tag_scope;

// sc は，それまでのスコープ情報を一時保存
// leave時に，一時保存していたscをもとに戻す．
static Scope *enter_scope() {
  Scope *sc     = calloc(1, sizeof(Scope));
  sc->var_scope = var_scope;
  sc->tag_scope = tag_scope;
  return sc;
}

static void leave_scope(Scope *sc) {
  var_scope = sc->var_scope;
  tag_scope = sc->tag_scope;
}

// 変数を名前で検索する．見つからなかった場合は，NULLを返す．
static VarScope *find_var(Token *tok) {
  for (VarScope *sc = var_scope; sc; sc = sc->next) {
    if (strlen(sc->name) == tok->len
        && !strncmp(tok->str, sc->name, tok->len)) {
      return sc;
    }
  }
  return NULL;
}

static TagScope *find_tag(Token *tok) {
  for (TagScope *sc = tag_scope; sc; sc = sc->next) {
    if (strlen(sc->name) == tok->len
        && !strncmp(tok->str, sc->name, tok->len)) {
      return sc;
    }
  }
  return NULL;
}

static Node *new_node(NodeKind kind, Token *tok) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->tok  = tok;
  return node;
}

static Node *new_binary(NodeKind kind,
                        Node *lhs,
                        Node *rhs,
                        Token *tok) {
  Node *node = new_node(kind, tok);
  node->lhs  = lhs;
  node->rhs  = rhs;
  return node;
}

static Node *new_unary(NodeKind kind,
                       Node *expr,
                       Token *tok) {
  Node *node = new_node(kind, tok);
  node->lhs  = expr;
  return node;
}

static Node *new_num(int val, Token *tok) {
  Node *node = new_node(ND_NUM, tok);
  node->val  = val;
  return node;
}

static Node *new_var_node(Var *var, Token *tok) {
  Node *node = new_node(ND_VAR, tok);
  node->var  = var;
  return node;
}

static VarScope *push_scope(char *name) {
  VarScope *sc = calloc(1, sizeof(VarScope));
  sc->name     = name;
  sc->next     = var_scope;
  var_scope    = sc;
  return sc;
}

static Var *new_var(char *name, Type *ty, bool is_local) {
  Var *var      = calloc(1, sizeof(Var));
  var->name     = name;
  var->ty       = ty;
  var->is_local = is_local;
  return var;
}

static Var *new_lvar(char *name, Type *ty) {
  Var *var              = new_var(name, ty, true);
  push_scope(name)->var = var;

  VarList *vl = calloc(1, sizeof(VarList));
  vl->var     = var;
  vl->next    = locals;
  locals      = vl;
  return var;
}

static Var *new_gvar(char *name, Type *ty, bool emit) {
  Var *var              = new_var(name, ty, false);
  push_scope(name)->var = var;

  // 変数かどうかをemitで判断？
  // 関数のときには，emit = false
  if (emit) {
    VarList *vl = calloc(1, sizeof(VarList));
    vl->var     = var;
    vl->next    = globals;
    globals     = vl;
  }
  return var;
}

static Type *find_typedef(Token *tok) {
  if (tok->kind == TK_IDENT) {
    VarScope *sc = find_var(tok);
    if (sc) {
      return sc->type_def;
    }
  }
  return NULL;
}

static char *new_label() {
  static int cnt = 0;
  char buf[20];
  sprintf(buf, ".L.data.%d", cnt++);
  return duplicate(buf, 20);
}

static Function *function();
static Type *basetype(bool *is_typedef);
static Type *declarator(Type *ty, char **name);
static Type *type_suffix(Type *ty);
static Type *struct_decl();
static Member *struct_member();
static void global_var();
static Node *declaration();
static bool is_typename();
static Node *stmt();
static Node *stmt2();
static Node *expr();
static Node *assign();
static Node *equality();
static Node *relational();
static Node *add();
static Node *mul();
static Node *unary();
static Node *postfix();
static Node *primary();

// 次のトップレベルに置かれているものが関数か
// グローバル変数化をトークンを先読みして判断する．
static bool is_function() {
  Token *tok = token;

  bool is_typedef;
  Type *ty   = basetype(&is_typedef);
  char *name = NULL;
  declarator(ty, &name);
  // 識別子+( と続いていたら，関数
  bool isfunc = name && consume("(");

  token = tok;
  return isfunc;
}

// program = (global-var | function)*
Program *program() {
  Function head = {};
  Function *cur = &head;
  globals       = NULL;

  while (!at_eof()) {
    if (is_function()) {
      Function *fn = function();
      if (!fn) {
        continue;
      }
      cur->next = fn;
      cur       = cur->next;
      continue;
    }
    global_var();
  }

  Program *prog = calloc(1, sizeof(Program));
  prog->globals = globals;
  prog->fns     = head.next;
  return prog;
}

// basetype = builtin-type | struct-decl | typedef-name
// builtin-type = "void" | "_Bool" | "char"
//              | "short" | "int" | "long" | "long" "long"
static Type *basetype(bool *is_typedef) {
  if (!is_typename()) {
    error_tok(token, "typename expected");
  }

  enum {
    VOID  = 1 << 0,
    BOOL  = 1 << 2,
    CHAR  = 1 << 4,
    SHORT = 1 << 6,
    INT   = 1 << 8,
    LONG  = 1 << 10,
    OTHER = 1 << 12,
  };

  Type *ty    = int_type;
  int counter = 0;

  if (is_typedef) {
    *is_typedef = false;
  }

  while (is_typename()) {
    Token *tok = token;

    if (consume("typedef")) {
      if (!is_typedef) {
        error_tok(tok, "invalid storage class specifier");
      }
      *is_typedef = true;
      continue;
    }

    // user defined type
    if (!peek("void") && !peek("_Bool") && !peek("char")
        && !peek("short") && !peek("int")
        && !peek("long")) {
      if (counter) {
        break;
      }

      if (peek("struct")) {
        ty = struct_decl();
      } else {
        ty = find_typedef(token);
        assert(ty);
        token = token->next;
      }

      counter |= OTHER;
      continue;
    }

    // handle built-in types;
    if (consume("void")) {
      counter += VOID;
    } else if (consume("_Bool")) {
      counter += BOOL;
    } else if (consume("char")) {
      counter += CHAR;
    } else if (consume("short")) {
      counter += SHORT;
    } else if (consume("int")) {
      counter += INT;
    } else if (consume("long")) {
      counter += LONG;
    }

    switch (counter) {
      case VOID:
        ty = void_type;
        break;
      case BOOL:
        ty = bool_type;
        break;
      case CHAR:
        ty = char_type;
        break;
      case SHORT:
      case SHORT + INT:
        ty = short_type;
        break;
      case INT:
        ty = int_type;
        break;
      case LONG:
      case LONG + INT:
      case LONG + LONG:
      case LONG + LONG + INT:
        ty = long_type;
        break;
      default:
        error_tok(tok, "invalid type");
    }
  }
  return ty;
}

// declarator = "*"* ("(" declarator ")" | ident)
// type-suffix
static Type *declarator(Type *ty, char **name) {
  while (consume("*")) {
    ty = pointer_to(ty);
  }
  if (consume("(")) {
    Type *placeholder = calloc(1, sizeof(Type));
    Type *new_ty      = declarator(placeholder, name);
    expect(")");
    memcpy(placeholder, type_suffix(ty), sizeof(Type));
    return new_ty;
  }
  *name = expect_ident();
  return type_suffix(ty);
}

// type-suffix = ("[" num "]" type-suffix)?
static Type *type_suffix(Type *ty) {
  if (!consume("[")) {
    return ty;
  }
  int array_size = expect_number();
  expect("]");
  ty = type_suffix(ty);
  return array_of(ty, array_size);
}

static void push_tag_scope(Token *tok, Type *ty) {
  TagScope *sc = calloc(1, sizeof(TagScope));
  sc->next     = tag_scope;
  sc->name     = duplicate(tok->str, tok->len);
  sc->ty       = ty;
  tag_scope    = sc;
}

// struct-decl = "struct" idnet
//             | "struct" ident? "{" struct-member "}"
static Type *struct_decl() {
  // Read struct tag
  expect("struct");
  Token *tag = consume_ident();
  if (tag && !peek("{")) {
    TagScope *sc = find_tag(tag);
    if (!sc) {
      error_tok(tag, "unknown struct type");
    }
    return sc->ty;
  }
  expect("{");

  // Read struct members.
  Member head = {};
  Member *cur = &head;

  while (!consume("}")) {
    cur->next = struct_member();
    cur       = cur->next;
  }

  Type *ty    = calloc(1, sizeof(Type));
  ty->kind    = TY_STRUCT;
  ty->members = head.next;

  // structのメンバのoffsetを設定する．
  int offset = 0;
  for (Member *mem = ty->members; mem; mem = mem->next) {
    offset      = align_to(offset, mem->ty->align);
    mem->offset = offset;
    offset += mem->ty->size;

    if (ty->align < mem->ty->align) {
      ty->align = mem->ty->align;
    }
  }
  ty->size = align_to(offset, ty->align);

  // nameが与えられていた場合は，struct typeを登録する．
  if (tag) {
    push_tag_scope(tag, ty);
  }
  return ty;
}

// struct-member = basetype ident ("[" num "]")* ";"
static Member *struct_member() {
  Type *ty   = basetype(NULL);
  char *name = NULL;
  ty         = declarator(ty, &name);
  ty         = type_suffix(ty);
  expect(";");

  Member *mem = calloc(1, sizeof(Member));
  mem->name   = name;
  mem->ty     = ty;
  return mem;
}

static VarList *read_func_param() {
  Type *ty   = basetype(NULL);
  char *name = NULL;
  ty         = declarator(ty, &name);
  ty         = type_suffix(ty);

  VarList *vl = calloc(1, sizeof(VarList));
  vl->var     = new_lvar(name, ty);
  return vl;
}

static VarList *read_func_params() {
  if (consume(")"))
    return NULL;

  VarList *head = read_func_param();
  VarList *cur  = head;

  while (!consume(")")) {
    expect(",");
    cur->next = read_func_param();
    cur       = cur->next;
  }

  return head;
}

// function = basetype declarator "(" params? ")"
//            ("{" stmt* "}" | ";")
// params   = param ("," param)*
// param    = basetype declarator type-suffix
Function *function() {
  locals = NULL;

  Type *ty   = basetype(NULL);
  char *name = NULL;
  ty         = declarator(ty, &name);

  // add a function type to the scope
  new_gvar(name, func_type(ty), false);

  // construct a function object
  Function *fn = calloc(1, sizeof(Function));
  fn->name     = name;
  expect("(");

  // 現状のscopeを一時保存
  Scope *sc  = enter_scope();
  fn->params = read_func_params();

  if (consume(";")) {
    leave_scope(sc);
    return NULL;
  }

  // function の中身を読み取る
  Node head = {};
  Node *cur = &head;
  expect("{");
  while (!consume("}")) {
    cur->next = stmt();
    cur       = cur->next;
  }
  // 一時保存していたscopeを戻して,
  // 一時保存からここまでで作られた変数の情報を開放
  leave_scope(sc);

  fn->node   = head.next;
  fn->locals = locals;
  return fn;
}

// global_var = basetype declarator type-suffix ";"
static void global_var() {
  bool is_typedef;
  Type *ty   = basetype(&is_typedef);
  char *name = NULL;
  ty         = declarator(ty, &name);
  ty         = type_suffix(ty);
  expect(";");

  if (is_typedef) {
    push_scope(name)->type_def = ty;
  } else {
    new_gvar(name, ty, true);
  }
}

// declaration = basetype declarator type-suffix
//              ("=" expr)? ";"
//            | basetype ";"
static Node *declaration() {
  Token *tok = token;
  bool is_typedef;
  Type *ty = basetype(&is_typedef);
  if (consume(";")) {
    return new_node(ND_NULL, tok);
  }

  char *name = NULL;
  ty         = declarator(ty, &name);
  ty         = type_suffix(ty);

  if (is_typedef) {
    expect(";");
    push_scope(name)->type_def = ty;
    return new_node(ND_NULL, tok);
  }

  if (ty->kind == TY_VOID) {
    error_tok(tok, "variable dclared void");
  }

  Var *var = new_lvar(name, ty);

  if (consume(";")) {
    return new_node(ND_NULL, tok);
  }

  expect("=");
  Node *lhs = new_var_node(var, tok);
  Node *rhs = expr();
  expect(";");
  Node *node = new_binary(ND_ASSIGN, lhs, rhs, tok);
  return new_unary(ND_EXPR_STMT, node, tok);
}

static Node *read_expr_stmt(void) {
  Token *tok = token;
  return new_unary(ND_EXPR_STMT, expr(), tok);
}

// 次のトークンが型を表していればtrue
static bool is_typename() {
  return peek("void") || peek("_Bool") || peek("char")
         || peek("short") || peek("int") || peek("long")
         || peek("struct") || peek("typedef")
         || find_typedef(token);
}

static Node *stmt() {
  Node *node = stmt2();
  add_type(node);
  return node;
}

// stmt2 = "return" expr ";"
//       | "if" "(" expr ")" stmt ("else" stmt)?
//       | "while" "(" expr ")" stmt
//       | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//       | "{" stmt* "}"
//       | declaration
//       | expr ";"
Node *stmt2() {
  Token *tok;
  if ((tok = consume("return"))) {
    Node *node = new_unary(ND_RETURN, expr(), tok);
    expect(";");
    return node;
  }

  if ((tok = consume("if"))) {
    Node *node = new_node(ND_IF, tok);
    expect("(");
    node->cond = expr();
    expect(")");
    node->then = stmt();
    if (consume("else")) {
      node->els = stmt();
    }
    return node;
  }

  if ((tok = consume("while"))) {
    Node *node = new_node(ND_WHILE, tok);
    expect("(");
    node->cond = expr();
    expect(")");
    node->then = stmt();
    return node;
  }

  if ((tok = consume("for"))) {
    Node *node = new_node(ND_FOR, tok);
    expect("(");
    if (!consume(";")) {
      node->init = read_expr_stmt();
      expect(";");
    }
    if (!consume(";")) {
      node->cond = expr();
      expect(";");
    }
    if (!consume(")")) {
      node->inc = read_expr_stmt();
      expect(")");
    }
    node->then = stmt();
    return node;
  }

  if ((tok = consume("{"))) {
    Node head = {};
    Node *cur = &head;

    // ブロックには入る前までの変数の宣言状態を一時保存
    Scope *sc = enter_scope();
    while (!consume("}")) {
      cur->next = stmt();
      cur       = cur->next;
    }
    // ブロックから出たので，一時保存していた変数の宣言状態を復旧
    leave_scope(sc);

    Node *node = new_node(ND_BLOCK, tok);
    node->body = head.next;
    return node;
  }

  if (is_typename()) {
    return declaration();
  }

  Node *node = read_expr_stmt();
  expect(";");
  return node;
}

// expr = equality
Node *expr() {
  return assign();
}

Node *assign() {
  Node *node = equality();
  Token *tok;
  if ((tok = consume("=")))
    node = new_binary(ND_ASSIGN, node, assign(), tok);
  return node;
}

// equality = relational ("==" relational | "!="
// relational)*
Node *equality() {
  Node *node = relational();
  Token *tok;

  for (;;) {
    if ((tok = consume("==")))
      node = new_binary(ND_EQ, node, relational(), tok);
    else if ((tok = consume("!=")))
      node = new_binary(ND_NE, node, relational(), tok);
    else
      return node;
  }
}

// relational = add("<" add | "<=" add | "<=" add)*
Node *relational() {
  Node *node = add();
  Token *tok;

  for (;;) {
    if ((tok = consume("<")))
      node = new_binary(ND_LT, node, add(), tok);
    else if ((tok = consume("<=")))
      node = new_binary(ND_LE, node, add(), tok);
    else if ((tok = consume(">")))
      node = new_binary(ND_LT, add(), node, tok);
    else if ((tok = consume(">=")))
      node = new_binary(ND_LE, add(), node, tok);
    else
      return node;
  }
}

static Node *new_add(Node *lhs, Node *rhs, Token *tok) {
  add_type(lhs);
  add_type(rhs);

  if (is_integer(lhs->ty) && is_integer(rhs->ty)) {
    return new_binary(ND_ADD, lhs, rhs, tok);
  }
  if (lhs->ty->base && is_integer(rhs->ty)) {
    return new_binary(ND_PTR_ADD, lhs, rhs, tok);
  }
  if (is_integer(lhs->ty) && rhs->ty->base) {
    return new_binary(ND_PTR_ADD, rhs, lhs, tok);
  }
  error_tok(tok, "invalid operands");
  return NULL;
}

static Node *new_sub(Node *lhs, Node *rhs, Token *tok) {
  add_type(lhs);
  add_type(rhs);

  if (is_integer(lhs->ty) && is_integer(rhs->ty)) {
    return new_binary(ND_SUB, lhs, rhs, tok);
  }
  if (lhs->ty->base && is_integer(rhs->ty)) {
    return new_binary(ND_PTR_SUB, lhs, rhs, tok);
  }
  if (lhs->ty->base && rhs->ty->base) {
    return new_binary(ND_PTR_DIFF, lhs, rhs, tok);
  }
  error_tok(tok, "invalid operands");
  return NULL;
}

// add = mul ("+" mul | "-" mul)*
Node *add() {
  Node *node = mul();
  Token *tok;

  for (;;) {
    if ((tok = consume("+")))
      node = new_add(node, mul(), tok);
    else if ((tok = consume("-")))
      node = new_sub(node, mul(), tok);
    else
      return node;
  }
}

Node *mul() {
  Node *node = unary();
  Token *tok;

  for (;;) {
    if ((tok = consume("*"))) {
      node = new_binary(ND_MUL, node, unary(), tok);
    } else if ((tok = consume("/"))) {
      node = new_binary(ND_DIV, node, unary(), tok);
    } else {
      return node;
    }
  }
}

// unary = ("+" | "-" | "*" | "&")? unary
//       | postfix
Node *unary() {
  Token *tok;
  if (consume("+"))
    return unary();
  if ((tok = consume("-")))
    return new_binary(
        ND_SUB, new_num(0, tok), unary(), tok);
  if ((tok = consume("&")))
    return new_unary(ND_ADDR, unary(), tok);
  if ((tok = consume("*")))
    return new_unary(ND_DEREF, unary(), tok);
  return postfix();
}

static Member *find_member(Type *ty, char *name) {
  for (Member *mem = ty->members; mem; mem = mem->next) {
    if (!strcmp(mem->name, name)) {
      return mem;
    }
  }
  return NULL;
}

static Node *struct_ref(Node *lhs) {
  add_type(lhs);
  if (lhs->ty->kind != TY_STRUCT) {
    error_tok(lhs->tok, "not a struct");
  }

  Token *tok  = token;
  Member *mem = find_member(lhs->ty, expect_ident());
  if (!mem) {
    error_tok(tok, "no such member");
  }

  Node *node   = new_unary(ND_MEMBER, lhs, tok);
  node->member = mem;
  return node;
}

// postfix = primary ("[" expr "]" | "." ident
//                   |"->" ident)*
static Node *postfix() {
  Node *node = primary();
  Token *tok;

  for (;;) {
    if ((tok = consume("["))) {
      // x[y] は *(x+y)の省略
      Node *exp = new_add(node, expr(), tok);
      expect("]");
      node = new_unary(ND_DEREF, exp, tok);
      continue;
    }

    if ((tok = consume("."))) {
      node = struct_ref(node);
      continue;
    }

    if ((tok = consume("->"))) {
      // x->y is short for (*x).y
      node = new_unary(ND_DEREF, node, tok);
      node = struct_ref(node);
      continue;
    }

    return node;
  }
}

// stmt-expr = "(" "{" stmt stmt* "}" ")"
//
// Statement expression は， GNUCの拡張です．
static Node *stmt_expr(Token *tok) {
  // stmt_exprに入る前までの変数の状態を一時保存
  Scope *sc = enter_scope();

  Node *node = new_node(ND_STMT_EXPR, tok);
  node->body = stmt();
  Node *cur  = node->body;

  while (!consume("}")) {
    cur->next = stmt();
    cur       = cur->next;
  }
  expect(")");

  // スコープから外れる際に，一時保存していた変数の状態を復旧
  leave_scope(sc);

  if (cur->kind != ND_EXPR_STMT) {
    error_tok(
        cur->tok,
        "stmt expr はvoidを返すことをサポートしてません．");
  }
  memcpy(cur, cur->lhs, sizeof(Node));
  return node;
}

char *duplicate(char *str, size_t len) {
  char *buffer = calloc(len + 1, sizeof(char));
  memcpy(buffer, str, len);
  buffer[len] = '\0';
  return buffer;
}

// func-args = "(" (assign ("," assign)*)? ")"
static Node *func_args(void) {
  if (consume(")"))
    return NULL;

  Node *head = assign();
  Node *cur  = head;
  while (consume(",")) {
    cur->next = assign();
    cur       = cur->next;
  }
  expect(")");
  return head;
}

// primary = "(" "{" stmt-expr-tail
//         | "(" expr ")"
//         | "sizeof" unary
//         | ident func-args?
//         | str
//         | num
Node *primary() {
  Token *tok;

  // 次のトークンが"("なら，"(" expr ")"のはず
  if ((tok = consume("("))) {
    if (consume("{")) {
      return stmt_expr(tok);
    }
    Node *node = expr();
    expect(")");
    return node;
  }

  if ((tok = consume("sizeof"))) {
    Node *node = unary();
    add_type(node);
    return new_num(node->ty->size, tok);
  }

  if ((tok = consume_ident())) {
    // Function call
    if (consume("(")) {
      Node *node      = new_node(ND_FUNC_CALL, tok);
      node->func_name = duplicate(tok->str, tok->len);
      node->args      = func_args();
      add_type(node);

      VarScope *sc = find_var(tok);
      if (sc) {
        if (!sc->var || sc->var->ty->kind != TY_FUNC) {
          error_tok(tok, "not a function");
        }
        node->ty = sc->var->ty->return_ty;
      } else {
        warn_tok(node->tok,
                 "implicit declaration of function");
        node->ty = int_type;
      }
      return node;
    }

    // 変数
    VarScope *sc = find_var(tok);
    if (sc && sc->var) {
      return new_var_node(sc->var, tok);
    }
    error_tok(tok, "undefined variable");
  }

  tok = token;
  if (tok->kind == TK_STR) {
    token = token->next;

    Type *ty      = array_of(char_type, tok->cont_len);
    Var *var      = new_gvar(new_label(), ty, true);
    var->contents = tok->contents;
    var->cont_len = tok->cont_len;
    return new_var_node(var, tok);
  }

  if (tok->kind != TK_NUM)
    error_tok(tok, "expected expression");
  return new_num(expect_number(), tok);
}


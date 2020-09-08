#include "kmcc.h"

// すべてのローカル変数
static VarList *locals;

// 変数を名前で検索する．見つからなかった場合は，NULLを返す．
static Var *find_var(Token *tok) {
  for (VarList *vl = locals; vl; vl = vl->next) {
    Var *var = vl->var;
    if (strlen(var->name) == tok->len &&
        !strncmp(tok->str, var->name, tok->len)) {
      return var;
    }
  }
  return NULL;
}

static Node *new_node(NodeKind kind, Token *tok) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->tok = tok;
  return node;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *tok) {
  Node *node = new_node(kind, tok);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

static Node *new_unary(NodeKind kind, Node *expr, Token *tok) {
  Node *node = new_node(kind, tok);
  node->lhs = expr;
  return node;
}

static Node *new_num(int val, Token *tok) {
  Node *node = new_node(ND_NUM, tok);
  node->val = val;
  return node;
}

static Node *new_var_node(Var *var, Token *tok) {
  Node *node = new_node(ND_VAR, tok);
  node->var = var;
  return node;
}

static Var *new_lvar(char *name) {
  Var *var = calloc(1, sizeof(Var));
  var->name = name;

  VarList *vl = calloc(1, sizeof(VarList));
  vl->var = var;
  vl->next = locals;
  locals = vl;
  return var;
}
static Function *function();
static Node *stmt();
static Node *stmt2();
static Node *expr();
static Node *assign();
static Node *equality();
static Node *relational();
static Node *add();
static Node *mul();
static Node *unary();
static Node *primary();

// program = function*
Function *program() {
  Function head = {};
  Function *cur = &head;

  while (!at_eof()) {
    cur->next = function();
    cur = cur->next;
  }
  return head.next;
}

static VarList *read_func_params() {
  if (consume(")")) return NULL;

  VarList *head = calloc(1, sizeof(VarList));
  head->var = new_lvar(expect_ident());
  VarList *cur = head;

  while (!consume(")")) {
    expect(",");
    cur->next = calloc(1, sizeof(VarList));
    cur->next->var = new_lvar(expect_ident());
    cur = cur->next;
  }

  return head;
}

// function = ident "(" params? ")" "{" stmt* "}"
// params   = idnet ("," ident)*
Function *function() {
  locals = NULL;

  Function *fn = calloc(1, sizeof(Function));
  fn->name = expect_ident();
  expect("(");
  fn->params = read_func_params();
  expect("{");

  Node head = {};
  Node *cur = &head;

  while (!consume("}")) {
    cur->next = stmt();
    cur = cur->next;
  }

  fn->node = head.next;
  fn->locals = locals;
  return fn;
}

static Node *read_expr_stmt(void) {
  Token *tok = token;
  return new_unary(ND_EXPR_STMT, expr(), tok);
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

    while (!consume("}")) {
      cur->next = stmt();
      cur = cur->next;
    }

    Node *node = new_node(ND_BLOCK, tok);
    node->body = head.next;
    return node;
  }

  Node *node = read_expr_stmt();
  expect(";");
  return node;
}

// expr = equality
Node *expr() { return assign(); }

Node *assign() {
  Node *node = equality();
  Token *tok;
  if ((tok = consume("="))) node = new_binary(ND_ASSIGN, node, assign(), tok);
  return node;
}

// equality = relational ("==" relational | "!=" relational)*
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
    if ((tok = consume("*")))
      node = new_binary(ND_MUL, node, unary(), tok);
    else if ((tok = consume("/")))
      node = new_binary(ND_DIV, node, unary(), tok);
    else
      return node;
  }
}

// unary = "+"? primary
//       | "-"? primary
//       | "*" unary
//       | "&" unary
Node *unary() {
  Token *tok;
  if (consume("+")) return unary();
  if ((tok = consume("-")))
    return new_binary(ND_SUB, new_num(0, tok), unary(), tok);
  if ((tok = consume("&"))) return new_unary(ND_ADDR, unary(), tok);
  if ((tok = consume("*"))) return new_unary(ND_DEREF, unary(), tok);
  return primary();
}

char *duplicate(char *str, size_t len) {
  char *buffer = calloc(len + 1, sizeof(char));
  memcpy(buffer, str, len);
  buffer[len] = '\0';
  return buffer;
}

// func-args = "(" (assign ("," assign)*)? ")"
static Node *func_args(void) {
  if (consume(")")) return NULL;

  Node *head = assign();
  Node *cur = head;
  while (consume(",")) {
    cur->next = assign();
    cur = cur->next;
  }
  expect(")");
  return head;
}

// primary = "(" expr ")" | ident func-args? | num
Node *primary() {
  // 次のトークンが"("なら，"(" expr ")"のはず
  if (consume("(")) {
    Node *node = expr();
    expect(")");
    return node;
  }

  Token *tok;
  if ((tok = consume_ident())) {
    // Function call
    if (consume("(")) {
      Node *node = new_node(ND_FUNC_CALL, tok);
      node->func_name = duplicate(tok->str, tok->len);
      node->args = func_args();
      return node;
    }

    // Identifier
    Var *var = find_var(tok);
    if (!var) {
      char *name = duplicate(tok->str, tok->len);
      var = new_lvar(name);
    }
    return new_var_node(var, tok);
  }

  tok = token;
  if (tok->kind != TK_NUM) error_tok(tok, "expected expression");
  return new_num(expect_number(), tok);
}


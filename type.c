#include "kmcc.h"

// char_type->kind = TY_CHAR
// char_type->size = 1
// char_type->align = 1
Type *void_type  = &(Type){TY_VOID, 1, 1};
Type *bool_type  = &(Type){TY_BOOL, 1, 1};
Type *char_type  = &(Type){TY_CHAR, 1, 1};
Type *short_type = &(Type){TY_SHORT, 2, 2};
Type *int_type   = &(Type){TY_INT, 4, 4};
Type *long_type  = &(Type){TY_LONG, 8, 8};

bool is_integer(Type *ty) {
  return ty->kind == TY_BOOL || ty->kind == TY_CHAR
         || ty->kind == TY_SHORT || ty->kind == TY_INT
         || ty->kind == TY_LONG;
}

// nは現在のoffset
// alignで指定したサイズに整えた値を出す
// n = 1, align = 8 return 8;
int align_to(int n, int align) {
  // ~ はbit反転演算子
  // & はbit演算のAND
  // n = 14; align = 8のとき
  // lhs = n + align - 1 = 21 = 100101
  // rhs = ~(align - 1) = ~7 = ~(000111) = 111000
  // return = 100101 & 111000 = 100000 = 16
  return (n + align - 1) & ~(align - 1);
}

static Type *new_type(TypeKind kind, int size, int align) {
  Type *ty  = calloc(1, sizeof(Type));
  ty->kind  = kind;
  ty->size  = size;
  ty->align = align;
  return ty;
}

Type *pointer_to(Type *base) {
  Type *ty = new_type(TY_PTR, 8, 8);
  ty->base = base;
  return ty;
}

Type *array_of(Type *base, int len) {
  Type *ty
      = new_type(TY_ARRAY, base->size * len, base->align);
  ty->base      = base;
  ty->array_len = len;
  return ty;
}

Type *func_type(Type *return_ty) {
  Type *ty      = new_type(TY_FUNC, 1, 1);
  ty->return_ty = return_ty;
  return ty;
}

void add_type(Node *node) {
  if (!node || node->ty)
    return;

  add_type(node->lhs);
  add_type(node->rhs);
  add_type(node->cond);
  add_type(node->then);
  add_type(node->els);
  add_type(node->init);
  add_type(node->inc);

  for (Node *n = node->body; n; n = n->next) {
    add_type(n);
  }
  for (Node *n = node->args; n; n = n->next) {
    add_type(n);
  }

  switch (node->kind) {
    case ND_ADD:
    case ND_SUB:
    case ND_PTR_DIFF:
    case ND_MUL:
    case ND_DIV:
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
    case ND_NUM:
      node->ty = long_type;
      return;
    case ND_PTR_ADD:
    case ND_PTR_SUB:
    case ND_ASSIGN:
      node->ty = node->lhs->ty;
      return;
    case ND_VAR:
      node->ty = node->var->ty;
      return;
    case ND_MEMBER:
      node->ty = node->member->ty;
      return;
    case ND_ADDR:
      if (node->lhs->ty->kind == TY_ARRAY) {
        node->ty = pointer_to(node->lhs->ty->base);
      } else {
        node->ty = pointer_to(node->lhs->ty);
      }
      return;
    case ND_DEREF:
      // baseがNULLということは，arrayでもptrでもない
      if (!node->lhs->ty->base) {
        error_tok(node->tok, "invalid pointer deference");
      }
      node->ty = node->lhs->ty->base;
      if (node->ty->kind == TY_VOID) {
        error_tok(node->tok,
                  "dereferencing a void pointer");
      }
      return;
    case ND_STMT_EXPR: {
      Node *last = node->body;
      while (last->next) {
        last = last->next;
      }
      node->ty = last->ty;
      return;
    }
    default:
      break;
  }
}


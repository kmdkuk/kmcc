#include "kmcc.h"

// ABI規定の引数
// 1bitレジスタ
static char *argreg1[]
    = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
// 2bit レジスタ
static char *argreg2[]
    = {"di", "si", "dx", "cx", "r8w", "r9w"};
// 4bit レジスタ
static char *argreg4[]
    = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
// 8bitレジスタ
static char *argreg8[]
    = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

static int labelseq = 1;
static char *func_name;

static void gen(Node *node);

// スタックにノードのアドレスをプッシュする．
static void gen_addr(Node *node) {
  switch (node->kind) {
    case ND_VAR: {
      Var *var = node->var;
      if (var->is_local) {
        printf("  lea rax, [rbp-%d]\n", var->offset);
        printf("  push rax\n");
      } else {
        printf("  push offset %s\n", var->name);
      }
      return;
    }
    case ND_DEREF:
      gen(node->lhs);
      return;
    case ND_MEMBER:
      gen_addr(node->lhs);
      printf("  pop rax\n");
      printf("  add rax, %d\n", node->member->offset);
      printf("  push rax\n");
      return;
    default:
      break;
  }

  error_tok(node->tok, "not an lvalue");
}

static void gen_lval(Node *node) {
  if (node->ty->kind == TY_ARRAY) {
    error_tok(node->tok, "not an lvalue");
  }
  gen_addr(node);
}

static void load(Type *ty) {
  printf("  pop rax\n");
  if (ty->size == 1) {
    printf("  movsx rax, byte ptr [rax]\n");
  } else if (ty->size == 2) {
    printf("  movsx rax, word ptr [rax]\n");
  } else if (ty->size == 4) {
    printf("  movsxd rax, dword ptr [rax]\n");
  } else {
    assert(ty->size == 8);
    printf("  mov rax, [rax]\n");
  }
  printf("  push rax\n");
}

static void store(Type *ty) {
  printf("  pop rdi\n");
  printf("  pop rax\n");

  if (ty->kind == TY_BOOL) {
    printf("  cmp rdi, 0\n");
    printf("  setne dil\n");
    printf("  movzb rdi, dil\n");
  }

  if (ty->size == 1) {
    printf("  mov [rax], dil\n");
  } else if (ty->size == 2) {
    printf("  mov [rax], di\n");
  } else if (ty->size == 4) {
    printf("  mov [rax], edi\n");
  } else {
    assert(ty->size == 8);
    printf("  mov [rax], rdi\n");
  }
  printf("  push rdi\n");
}

void gen(Node *node) {
  switch (node->kind) {
    case ND_NULL:
      return;
    case ND_NUM:
      printf("  push %d\n", node->val);
      return;
    case ND_EXPR_STMT:
      gen(node->lhs);
      printf("  add rsp, 8\n");
      return;
    case ND_VAR:
    case ND_MEMBER:
      gen_addr(node);
      if (node->ty->kind != TY_ARRAY) {
        load(node->ty);
      }
      return;
    case ND_ASSIGN:
      gen_lval(node->lhs);
      gen(node->rhs);
      store(node->ty);
      return;
    case ND_ADDR:
      gen_addr(node->lhs);
      return;
    case ND_DEREF:
      gen(node->lhs);
      if (node->ty->kind != TY_ARRAY) {
        load(node->ty);
      }
      return;
    case ND_IF: {
      int seq = labelseq++;
      if (node->els) {
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je .L.else.%d\n", seq);
        gen(node->then);
        printf("  jmp .L.end.%d\n", seq);
        printf(".L.else.%d:\n", seq);
        gen(node->els);
        printf(".L.end.%d:\n", seq);
      } else {
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je .L.end.%d\n", seq);
        gen(node->then);
        printf(".L.end.%d:", seq);
      }
      return;
    }
    case ND_WHILE: {
      int seq = labelseq++;
      printf(".L.begin.%d:\n", seq);
      gen(node->cond);
      printf("  pop rax\n");
      printf("  cmp rax, 0\n");
      printf("  je .L.end.%d\n", seq);
      gen(node->then);
      printf("  jmp .L.begin.%d\n", seq);
      printf(".L.end.%d:\n", seq);
      return;
    }
    case ND_FOR: {
      int seq = labelseq++;
      if (node->init) {
        gen(node->init);
      }
      printf(".L.begin.%d:\n", seq);
      if (node->cond) {
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je .L.end.%d\n", seq);
      }
      gen(node->then);
      if (node->inc) {
        gen(node->inc);
      }
      printf("  jmp .L.begin.%d\n", seq);
      printf(".L.end.%d:\n", seq);
      return;
    }
    case ND_BLOCK:
    case ND_STMT_EXPR:
      for (Node *n = node->body; n; n = n->next) gen(n);
      return;
    case ND_FUNC_CALL: {
      int nargs = 0;
      for (Node *arg = node->args; arg; arg = arg->next) {
        gen(arg);
        nargs++;
      }
      for (int i = nargs - 1; i >= 0; i--) {
        printf("  pop %s\n", argreg8[i]);
      }
      // We need to align RSP to a 16 byte boundary before
      // calling a function because it is an ABI
      // requirement. RAX is set to 0 for variadic function.
      int seq = labelseq++;
      printf("  mov rax, rsp\n");
      printf("  and rax, 15\n");
      printf("  jnz .L.call.%d\n", seq);
      printf("  mov rax, 0\n");
      printf("  call %s\n", node->func_name);
      printf("  jmp .L.end.%d\n", seq);
      printf(".L.call.%d:\n", seq);
      printf("  sub rsp, 8\n");
      printf("  mov rax, 0\n");
      printf("  call %s\n", node->func_name);
      printf("  add rsp, 8\n");
      printf(".L.end.%d:\n", seq);
      printf("  push rax\n");
      return;
    }
    case ND_RETURN:
      gen(node->lhs);
      printf("  pop rax\n");
      printf("  jmp .L.return.%s\n", func_name);
      return;
    default:
      break;
  }

  gen(node->lhs);
  gen(node->rhs);

  printf("  pop rdi\n");
  printf("  pop rax\n");

  switch (node->kind) {
    case ND_ADD:
      printf("  add rax, rdi\n");
      break;
    case ND_PTR_ADD:
      printf("  imul rdi, %d\n", node->ty->base->size);
      printf("  add rax, rdi\n");
      break;
    case ND_SUB:
      printf("  sub rax, rdi\n");
      break;
    case ND_PTR_SUB:
      printf("  imul rdi, %d\n", node->ty->base->size);
      printf("  sub rax, rdi\n");
      break;
    case ND_PTR_DIFF:
      printf("  sub rax, rdi\n");
      printf("  cqo\n");
      printf("  mov rdi, %d\n", node->lhs->ty->base->size);
      printf("  idiv rdi\n");
      break;
    case ND_MUL:
      printf("  imul rax, rdi\n");
      break;
    case ND_DIV:
      printf("  cqo\n");
      printf("  idiv rdi\n");
      break;
    case ND_EQ:
      printf("  cmp rax, rdi\n");
      printf("  sete al\n");
      printf("  movzb rax, al\n");
      break;
    case ND_NE:
      printf("  cmp rax, rdi\n");
      printf("  setne al\n");
      printf("  movzb rax, al\n");
      break;
    case ND_LT:
      printf("  cmp rax, rdi\n");
      printf("  setl al\n");
      printf("  movzb rax, al\n");
      break;
    case ND_LE:
      printf("  cmp rax, rdi\n");
      printf("  setle al\n");
      printf("  movzb rax, al\n");
      break;
    default:
      break;
  }

  printf("  push rax\n");
}

static void emit_data(Program *prog) {
  printf(".data\n");

  for (VarList *vl = prog->globals; vl; vl = vl->next) {
    Var *var = vl->var;
    printf("%s:\n", var->name);

    if (!var->contents) {
      printf("  .zero %d\n", var->ty->size);
      continue;
    }

    for (int i = 0; i < var->cont_len; i++) {
      printf("  .byte %d\n", var->contents[i]);
    }
  }
}

static void load_arg(Var *var, int idx) {
  int size = var->ty->size;
  if (size == 1) {
    printf(
        "  mov [rbp-%d], %s\n", var->offset, argreg1[idx]);
  } else if (size == 2) {
    printf(
        "  mov [rbp-%d], %s\n", var->offset, argreg2[idx]);
  } else if (size == 4) {
    printf(
        "  mov [rbp-%d], %s\n", var->offset, argreg4[idx]);
  } else {
    assert(size == 8);
    printf(
        "  mov [rbp-%d], %s\n", var->offset, argreg8[idx]);
  }
}

static void emit_text(Program *prog) {
  printf(".text\n");

  for (Function *fn = prog->fns; fn; fn = fn->next) {
    func_name = fn->name;
    printf(".global %s\n", func_name);
    printf("%s:\n", func_name);

    // プロローグ
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", fn->stack_size);

    // 引数をスタックにプッシュ
    int i = 0;
    for (VarList *vl = fn->params; vl; vl = vl->next) {
      load_arg(vl->var, i++);
    }

    // コードの実行
    for (Node *node = fn->node; node; node = node->next)
      gen(node);

    // エピローグ
    printf(".L.return.%s:\n", func_name);
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
  }
}

void codegen(Program *prog) {
  printf(".intel_syntax noprefix\n");
  emit_data(prog);
  emit_text(prog);
}

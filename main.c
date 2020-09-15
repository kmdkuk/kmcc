#include <errno.h>
#include <stdio.h>

#include "kmcc.h"

// 指定されたファイルの内容を返す．
char *read_file(char *path) {
  FILE *fp = fopen(path, "r");
  if (!fp) {
    error_at("cannot open %s: %s", path, strerror(errno));
  }

  // ファイルの長さを調べる．
  if (fseek(fp, 0, SEEK_END) == -1) {
    error("%s: fseek: %s", path, strerror(errno));
  }
  size_t size = ftell(fp);
  if (fseek(fp, 0, SEEK_SET) == -1) {
    error("%s: fseek: %s", path, strerror(errno));
  }

  // ファイルの内容を読み込む
  char *buf = calloc(1, size + 1);
  fread(buf, size, 1, fp);

  // ファイルが必ず"\n\0"で終わっているようにする．
  if (size == 0 || buf[size - 1] != '\n') {
    buf[size++] = '\n';
  }
  buf[size] = '\0';
  fclose(fp);
  return buf;
}

int align_to(int n, int align) {
  return (n + align - 1) & ~(align - 1);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "引数の個数が正しくありません\n");
    return 1;
  }

  // トークナイズしてパースする．
  // filename      = argv[1];
  user_input    = argv[1];
  token         = tokenize();
  Program *prog = program();

  for (Function *fn = prog->fns; fn; fn = fn->next) {
    // ローカル変数のオフセットを割り当てる
    int offset = 0;
    for (VarList *vl = fn->locals; vl; vl = vl->next) {
      Var *var = vl->var;
      offset += var->ty->size;
      var->offset = offset;
    }
    fn->stack_size = align_to(offset, 8);
  }

  codegen(prog);

  return 0;
}

#include "kmcc.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "引数の個数が正しくありません\n");
    return 1;
  }

  // トークナイズしてパースする．
  user_input = argv[1];
  token = tokenize();
  Function *prog = program();

  // ローカル変数のオフセットを割り当てる
  int offset = 0;
  for (Var *var = prog->locals; var; var = var->next) {
    offset += 8;
    var->offset = offset;
  }
  prog->stack_size = offset;

  codegen(prog);

  return 0;
}
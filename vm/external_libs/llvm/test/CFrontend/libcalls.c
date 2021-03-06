// llvm-gcc -O1+ should run simplify libcalls, O0 shouldn't
// and -fno-builtins shouldn't.
// RUN: %llvmgcc %s -S -emit-llvm -O0 -o - | grep {call.*exp2f}
// RUN: %llvmgcc %s -S -emit-llvm -O1 -o - | grep {call.*ldexp}
// RUN: %llvmgcc %s -S -emit-llvm -O3 -fno-builtin -o - | grep {call.*exp2f}

float t4(unsigned char x) {
  return exp2f(x);
}


; RUN: llvm-as < %s | llc -march=x86-64 | grep movups | count 1

define <2 x i64> @bar(<2 x i64>* %p, <2 x i64> %x) nounwind {
  %t = load <2 x i64>* %p, align 8
  %z = mul <2 x i64> %t, %x
  ret <2 x i64> %z
}

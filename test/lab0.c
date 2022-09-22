#include "lib.h"

/**
 * Compare and Swap
 * @param dest
 * @param new_value
 * @param old_value
 * @return success `1' or not `0'
 */
int CAS(long* dest, long new_value, long old_value) {
  int ret = 0;
  asm volatile(
    "cas: lr.w t0,(%1)\n"
    "bne t0, %3, fail\n"
    "sc.w t1, %2,(%1)\n"
    "bnez t1, fail\n"
    "li %0,1\n"
    "j success \n"
    "fail: li %0,0\n"
    "success:"
    :"=r"(ret)
    :"r"(dest),
    "r"(new_value),
    "r"(old_value)
  );
  return ret;
}

static long dst;

int main() {
  int res;

  dst = 1;

  for (int i = 0; i < 2; ++i) {
    res = CAS(&dst, 211, i);
    if (res)
      print_s("CAS SUCCESS\n");
    else
      print_s("CAS FAIL\n");

    print_d(res);
    print_c('\n');
    print_d(dst);
    print_c('\n');
  }

  exit_proc();
}
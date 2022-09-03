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

  // TODO: write your code here


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
#include "lib.h"

int main() {

  char *a;
  a = 0x100000;
  a[0] = 'C';
  a[1] = 'S';
  a[6] = '\n';
  print_c(a[0]);
  print_c(a[1]);
  print_c(a[2]);
  print_c(a[3]);
  print_c(a[4]);
  print_c(a[5]);
  print_c(a[6]);

  exit_proc();
}
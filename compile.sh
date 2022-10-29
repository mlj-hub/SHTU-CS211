# ！/bin/sh
gcc-riscv -march=rv64i ~/RISCV-Simulator/test/lib.c ~/RISCV-Simulator/test/branch-test.c -o ~/RISCV-Simulator/riscv-elf/lab2.riscv
gcc-riscv-obj -D ~/RISCV-Simulator/riscv-elf/lab2.riscv > ~/RISCV-Simulator/riscv-elf/lab2.s

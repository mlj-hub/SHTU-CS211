# ！/bin/sh
gcc-riscv -march=rv64ia ~/RISCV-Simulator/test/lib.c ~/RISCV-Simulator/test/lab0.c -o ~/RISCV-Simulator/riscv-elf/lab0.riscv
gcc-riscv-obj -D ~/RISCV-Simulator/riscv-elf/lab0.riscv > ~/RISCV-Simulator/riscv-elf/lab0.s

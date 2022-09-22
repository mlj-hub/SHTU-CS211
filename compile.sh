# ！/bin/sh
gcc-riscv -march=rv64ia ~/workspace/cs211/lab/lab0/RISCV-Simulator/test/lib.c ~/workspace/cs211/lab/lab0/RISCV-Simulator/test/lab0.c -o ~/workspace/cs211/lab/lab0/RISCV-Simulator/riscv-elf/lab0.riscv
gcc-riscv-obj -D ~/workspace/cs211/lab/lab0/RISCV-Simulator/riscv-elf/lab0.riscv > ~/workspace/cs211/lab/lab0/RISCV-Simulator/riscv-elf/lab0.s

# ！/bin/sh
gcc-riscv -march=rv64ia ~/RISCV-Simulator/test/lib.c ~/RISCV-Simulator/test/lab3-core0.c -o ~/RISCV-Simulator/riscv-elf/lab3-core0.riscv
gcc-riscv-obj -D ~/RISCV-Simulator/riscv-elf/lab3-core0.riscv > ~/RISCV-Simulator/riscv-elf/lab3-core0.s
gcc-riscv -march=rv64ia ~/RISCV-Simulator/test/lib.c ~/RISCV-Simulator/test/lab3-core1.c -o ~/RISCV-Simulator/riscv-elf/lab3-core1.riscv
gcc-riscv-obj -D ~/RISCV-Simulator/riscv-elf/lab3-core1.riscv > ~/RISCV-Simulator/riscv-elf/lab3-core1.s

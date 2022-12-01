# !/bin/bash
cd build
cmake ..
make
./Simulator -c0 ../riscv-elf/lab3-core0.riscv -c1 ../riscv-elf/lab3-core1.riscv
# ./Simulator -c0 ../riscv-elf/lab3-core0.riscv -c1 ../riscv-elf/lab3-core1.riscv -v > log.txt

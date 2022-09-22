# !/bin/bash
cd build
cmake ..
make
./Simulator ../riscv-elf/lab0.riscv
./Simulator ../riscv-elf/lab0.riscv -v > ./log.txt

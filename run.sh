# !/bin/bash
cd build
cmake ..
make -j
./Simulator ../riscv-elf/lab2.riscv -b PERCEPTRON > ./perceptron.txt
./Simulator ../riscv-elf/lab2.riscv -b BPB > ./bpb.txt
./Simulator ../riscv-elf/lab2.riscv -b BTFNT > ./bftnt.txt
# ./Simulator ../riscv-elf/lab2.riscv -b NT
# ./Simulator ../riscv-elf/lab2.riscv -b AT
# ./Simulator ../riscv-elf/lab2.riscv -v > ./log.txt

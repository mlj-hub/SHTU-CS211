import os
main = "/home/malj/RISCV-Simulator/build/Simulator /home/malj/RISCV-Simulator/riscv-elf/matrixmulti.riscv \
        -b PERCEPTRON"
for h in range(30,41):
	for t in range(76,85):
		test = main+" -h "+str(h)+" -t "+str(t)+" >> ./a.txt"
		r_v = os.system(test)
		
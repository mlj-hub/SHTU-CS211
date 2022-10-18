import random
with open("cache-trace/test.trace",mode="w") as f:
    for j in range(1):
        for i in range(int("0x1fffff",16)):
            p = random.random()
            access = ""
            if p>=0.6:
                access='w'
            else:
                access="r"
            data = access+" "+str(i)+"\n"
            f.write(data)
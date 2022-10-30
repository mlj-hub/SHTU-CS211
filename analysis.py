# data = open("a.txt","r")
b=0
with open("a.txt","r") as data:
    with open("result.txt","w") as result:
        line = data.readline()
        while(line):
            # if(line[0]=='n'):
            #     a = line.split(":")
            #     result.write(str(int(a[1].strip("\n"))-1)+"&")
            # if(line[0]=="t"):
            #     a = line.split(":")
            #     result.write(a[1].strip("\n")+"&")
            if(line == "------------ STATISTICS -----------\n"):
                line = data.readline()
                line = data.readline()
                a = line.split(" ")
                # result.write(a[3]+"\\\\ \hline")
                result.write(a[3]+"&")
                b+=1
                if(b%9==0):
                    result.write("\\\\ \hline \n")
            line = data.readline()
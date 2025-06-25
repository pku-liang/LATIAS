import sys

path = sys.argv[1]
M = int(sys.argv[2])
N = int(sys.argv[3])
K = int(sys.argv[4])

with open(path, "r") as yaml_file:
    cnt = 0
    for line in yaml_file.readlines():
        if cnt == 0 and line.find("factors:") != -1:
            print(f"  factors: M={M // 128}, N={N // 128}, K={(K + 255) // 256}")
            cnt += 1
        elif cnt == 1 and line.find("factors:") != -1:
            if (K // 128) % 2 == 1:
                print(f"    factors: M=1, N=1, K=2,1")
            else:
                print(f"    factors: M=1, N=1, K=2")
            cnt += 1
        else:
            print(line, end="")
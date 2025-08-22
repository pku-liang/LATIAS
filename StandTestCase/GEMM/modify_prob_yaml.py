import sys

path = sys.argv[1]
M = int(sys.argv[2])
N = int(sys.argv[3])
K = int(sys.argv[4])

with open(path, "r") as yaml_file:
    for line in yaml_file.readlines():
        if line.find("M:") != -1:
            print(f"    M: {M}")
        elif line.find("N:") != -1:
            print(f"    N: {N}")
        elif line.find("K:") != -1:
            print(f"    K: {K}")
        else:
            print(line, end="")
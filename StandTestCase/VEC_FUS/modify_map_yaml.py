import sys

path = sys.argv[1]
M = int(sys.argv[2])
N = int(sys.argv[3])

with open(path, "r") as yaml_file:
    is_replaced = False
    for line in yaml_file.readlines():
        if not is_replaced and line.find("factors:") != -1:
            print(f"  factors: M={M // 128}, N={N // 128}")
            is_replaced = True
        else:
            print(line, end="")
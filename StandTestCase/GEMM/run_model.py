from concurrent.futures import ProcessPoolExecutor
import pandas as pd
import os

def task(M, N, K, out_csv):
    os.system(f"bash run_gemm.sh {M} {N} {K} {out_csv}")


df = pd.read_csv("gemm_new.csv")
out_csv = "model_data_test.csv"
os.system(f"echo \"M,N,K,latency(us)\" > {out_csv}")
with ProcessPoolExecutor(max_workers=64) as executor:
    # 提交单个任务
    tasks = []
    for row in df.itertuples():
        M = row.M
        N = row.N
        K = row.K
        process = executor.submit(task, M, N, K, out_csv)
        tasks.append(process)

    for process in tasks:
        process.result()
        print("Complete")




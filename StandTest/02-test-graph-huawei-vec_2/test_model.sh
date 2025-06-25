out_csv="model_data.csv"
yaml_dir=tests/02-test-graph-huawei-vec
out_yaml_path=tests/02-test-graph-huawei-vec/map.yaml
i=$1
j=$2

python modify_map_yaml.py $yaml_dir/map.yaml_ $i $j > $yaml_dir/map.yaml
python modify_prob_yaml.py $yaml_dir/prob.yaml_ $i $j > $yaml_dir/prob.yaml
./build/TileExp $yaml_dir/*.yaml > tmp.out
latency=$(tail -n 1 tmp.out | awk -F':' '{print $NF}')
echo "$i,$j,$latency"
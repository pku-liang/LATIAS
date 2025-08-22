yaml_dir="."
out_csv="$yaml_dir/model_data_fuse_vec_test.csv"

echo "M,N,latency(us)" > $out_csv

for((i=128; i<=4096; i+=128)); do
    for((j=128; j<=4096; j+=128)); do
        python $yaml_dir/modify_map_yaml.py $yaml_dir/map.yaml_ $i $j > $yaml_dir/map.yaml
        python $yaml_dir/modify_prob_yaml.py $yaml_dir/prob.yaml_ $i $j > $yaml_dir/prob.yaml
        ../../build/TileExp $yaml_dir/*.yaml > tmp.out
        latency=$(tail -n 1 tmp.out | awk -F':' '{print $NF}')
        echo "$i,$j,$latency" >> $out_csv
    done
done
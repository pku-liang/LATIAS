yaml_dir="."
out_csv="$yaml_dir/model_data1.csv"

echo "M,N,latency(us)" > $out_csv

for((i=128; i<=4096; i+=128)); do
    for((j=128; j<=4096; j+=128)); do
        dir=tmp/${i}_${j}
        mkdir -p $yaml_dir/$dir
        cp Huawei.yaml $yaml_dir/$dir/Huawei.yaml
        cp interconnection.yaml $yaml_dir/$dir/interconnection.yaml
        python $yaml_dir/modify_map_yaml.py $yaml_dir/map.yaml_ $i $j > $yaml_dir/$dir/map.yaml
        python $yaml_dir/modify_prob_yaml.py $yaml_dir/prob.yaml_ $i $j > $yaml_dir/$dir/prob.yaml
        ../../build/TileExp $yaml_dir/$dir/*.yaml > $yaml_dir/$dir/tmp.out
        latency=$(tail -n 1 $yaml_dir/$dir/tmp.out | awk -F':' '{print $NF}')
        echo "$i,$j,$latency" >> $out_csv
    done
done
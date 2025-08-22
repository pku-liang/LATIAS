M=$1
N=$2
K=$3
out_csv=$4
yaml_dir="."
dir=tmp/${M}_${N}_${K}

mkdir -p $yaml_dir/$dir
cp Huawei.yaml $yaml_dir/$dir/Huawei.yaml
cp interconnection.yaml $yaml_dir/$dir/interconnection.yaml
python $yaml_dir/modify_map_yaml.py $yaml_dir/map.yaml_ $M $N $K > $yaml_dir/$dir/map.yaml
python $yaml_dir/modify_prob_yaml.py $yaml_dir/prob.yaml_ $M $N $K  > $yaml_dir/$dir/prob.yaml
../../build/TileExp $yaml_dir/$dir/*.yaml > $yaml_dir/$dir/tmp.out
latency=$(tail -n 1 $yaml_dir/$dir/tmp.out | awk -F':' '{print $NF}')
echo "$M,$N,$K,$latency" >> $out_csv
rm -r $yaml_dir/$dir
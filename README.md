# TileExp
## Install

    sudo apt install scons libconfig++-dev libboost-dev libboost-iostreams-dev libboost-serialization-dev libyaml-cpp-dev libncurses-dev libtinfo-dev libgpm-dev git build-essential python3-pip


## Build
### Build timeloop
    cd 3rdparty/timeloop 
    scons -j16 --d --static 
### Build LATIAS 
    cd - 
    scons -j16 --d --static

## Run Test
    cd tests
    bash test.sh

## Tutorial
    Detailed tutorial will be released soon.

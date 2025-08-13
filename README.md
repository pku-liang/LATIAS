# TileExp
## Install

    sudo apt install scons libconfig++-dev libboost-dev libboost-iostreams-dev libboost-serialization-dev libyaml-cpp-dev libncurses-dev libtinfo-dev libgpm-dev git build-essential python3-pip


## Build
### Clone
    git clone git@github.com:ChengruiZhang/TileExp.git
    git switch LATIAS_V2

### Build timeloop
    cd 3rdparty/timeloop 
    scons -j16 --d --static 
### Build LATIAS 
    cd - 
    scons -j16 --d --static

## Run Test
bash test.sh

## Vscode C++ debug mode

Vscode debug json file -- launch.json

    {
        "name": "TileExp debug",
        "type": "cppdbg",   
        "request": "launch",
        "program": "{$workspace}/build/TileExp",
        "args": ["*.yaml"],
        "stopAtEntry": false,
        "cwd": "{$workspace}/tests/01-test-graph-huawei/",
        "environment": [],
        "externalConsole": false,
        "MIMode": "gdb",
        "setupCommands": [
            {
            "description": "Enable pretty-printing for gdb",
            "text": "-enable-pretty-printing",
            "ignoreFailures": true
            }
        ],
        "miDebuggerPath": "/usr/bin/gdb"
    },
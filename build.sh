cd 3rdparty/timeloop
scons -j128 --d --static

cd -
# scons -c
scons --static --d -j128


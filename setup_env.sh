#!/bin/sh
cmd="apt-get update"
echo "start: $cmd"
sudo $cmd

# rdma lib
cmd="apt-get install libibverbs-dev librdmacm-dev libibverbs1 ibverbs-utils librdmacm1 libibumad3 ibverbs-providers infiniband-diags rdma-core -y"
echo "start: $cmd"
sudo $cmd

# show the devicexs
cmd="ibv_devices"
echo "start: $cmd"
sudo $cmd

# iproute2
cmd="apt-get install iproute2 -y"
echo "start: $cmd"
sudo $cmd

# perftest
cmd="apt-get install perftest -y"
echo "start: $cmd"
sudo $cmd

# load rxe
cmd="modprobe rdma_rxe"
echo "start: $cmd"
sudo $cmd

# set Soft-RoCE
# change 'ens33' to your CA name
# 'rxe_0' is the new name, you can change it to whatever you want
cmd="rdma link add rxe_0 type rxe netdev ens33"
echo "start: $cmd"
sudo $cmd

# check if ACTIVE
cmd="rdma link"
echo "start: $cmd"
sudo $cmd


# install libevent
echo "start: install libevent"
git clone https://github.com/libevent/libevent.git
cd libevent
mkdir build && cd build
cmake ..
make 
sudo make install




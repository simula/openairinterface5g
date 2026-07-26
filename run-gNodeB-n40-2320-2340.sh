#!/usr/bin/bash -eu

# Assuming 210!

ping -c3 -i0.2 10.133.211.35   # AMF

#cd /root/openair5G
source oaienv
cd cmake_targets/ran_build/build
#nice -n -19 ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/dsb.conf --gNBs.[0].min_rxtxtime 6 --sa --usrp-tx-thread-config 1 -E
nice -n -19 ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/nkom-24269-n40-2320-2340.conf --gNBs.[0].min_rxtxtime 6 --usrp-tx-thread-config 1 -E
#nice -n -19 ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/dsb.conf.2300MHz --gNBs.[0].min_rxtxtime 6 --sa --usrp-tx-thread-config 1 -E



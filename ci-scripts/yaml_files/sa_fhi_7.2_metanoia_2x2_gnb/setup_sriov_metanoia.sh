set -e
sudo cpupower idle-set -D 0 > /dev/null
sudo sysctl kernel.sched_rt_runtime_us=-1
sudo sysctl kernel.timer_migration=0
sudo ethtool -G ens7f1 rx 8160 tx 8160
sudo sh -c 'echo 0 > /sys/class/net/ens7f1/device/sriov_numvfs'
sudo sh -c 'echo 3 > /sys/class/net/ens7f1/device/sriov_numvfs'
sudo modprobe -r iavf
sudo modprobe iavf
# this next 2 lines is for C/U planes
sudo ip link set ens7f1 vf 0 mac 00:11:22:33:44:55 vlan 3 spoofchk off mtu 9000
sudo ip link set ens7f1 vf 1 mac 00:11:22:33:44:54 vlan 3 spoofchk off mtu 9000
sleep 1
# These are the DPDK bindings for C/U-planes on vlan 1
sudo /usr/local/bin/dpdk-devbind.py --unbind c3:11.0
sudo /usr/local/bin/dpdk-devbind.py --unbind c3:11.1
sudo modprobe vfio-pci
sudo /usr/local/bin/dpdk-devbind.py --bind vfio-pci c3:11.0
sudo /usr/local/bin/dpdk-devbind.py --bind vfio-pci c3:11.1
exit 0

set -x

sudo mbimcli -p -d /dev/cdc-wdm0 --set-radio-state=off

IF=wwan0
sudo ip link set ${IF} down
sudo ip addr flush dev ${IF}

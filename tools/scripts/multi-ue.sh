#!/bin/bash

ue_id=-1

die() {
  echo "$@"
  exit 1
}

create_namespace() {
  ue_id=$1
  local name="ue$ue_id"
  echo "creating namespace for UE ID ${ue_id} name ${name}"
  ip netns add $name
  ip link add v-eth$ue_id type veth peer name v-ue$ue_id
  ip link set v-ue$ue_id netns $name
  BASE_IP=$((200+ue_id))
  ip addr add 10.$BASE_IP.1.100/24 dev v-eth$ue_id
  ip link set v-eth$ue_id up
  iptables -t nat -A POSTROUTING -s 10.$BASE_IP.1.0/255.255.255.0 -o lo -j MASQUERADE
  iptables -A FORWARD -i lo -o v-eth$ue_id -j ACCEPT
  iptables -A FORWARD -o lo -i v-eth$ue_id -j ACCEPT
  ip netns exec $name ip link set dev lo up
  ip netns exec $name ip addr add 10.$BASE_IP.1.$ue_id/24 dev v-ue$ue_id
  ip netns exec $name ip link set v-ue$ue_id up
}

delete_namespace() {
  local ue_id=$1
  local name="ue$ue_id"
  echo "deleting namespace for UE ID ${ue_id} name ${name}"
  ip link delete v-eth$ue_id
  ip netns delete $name
}

list_namespaces() {
  ip netns list
}

open_namespace() {
  [[ $ue_id -ge 1 ]] || die "error: no last UE processed"
  local name="ue$ue_id"
  echo "opening shell in namespace ${name}"
  echo "type 'ip netns exec $name bash' in additional terminals"
  ip netns exec $name bash
}

inter_namespace_communication() {
    [[ $# -eq 2 ]] || die "error: expected 2 UE IDs"
    local id1=$1
    local id2=$2

    [[ $id1 -ne $id2 ]] || die "error: UE IDs must be different"

    local ns1="ue$id1"
    local ns2="ue$id2"
    local if1="v-inter-ue_${id1}${id2}"
    local if2="v-inter-ue_${id2}${id1}"

    ip netns list | grep -q "$ns1" || die "error: namespace $ns1 does not exist"

    ip netns list | grep -q "$ns2" || die "error: namespace $ns2 does not exist"

    local combo1="${id1}${id2}"
    local combo2="${id2}${id1}"
    
    local subnet
    if [[ $combo1 -gt $combo2 ]]; then
        subnet=$combo1
    else
        subnet=$combo2
    fi

    echo "Creating connection between $ns1 and $ns2 with subnet 172.16.$subnet.0/24"
    ip link add $if1 type veth peer name $if2
    ip link set $if1 netns $ns1
    ip link set $if2 netns $ns2

    ip netns exec $ns1 ip addr add 172.16.$subnet.$combo1/24 dev $if1
    ip netns exec $ns2 ip addr add 172.16.$subnet.$combo2/24 dev $if2

    ip netns exec $ns1 ip link set dev $if1 up
    ip netns exec $ns2 ip link set dev $if2 up

    echo "Connection established between $ns1 ($ns1: 172.16.$subnet.$combo1) and $ns2 ($ns2: 172.16.$subnet.$combo2)"
}
usage () {
  echo "$1 -c <num>: create namespace \"ue<num>\""
  echo "$1 -d <num>: delete namespace \"ue<num>\""
  echo "$1 -e      : execute shell in last processed namespace"
  echo "$1 -l      : list namespaces"
  echo "$1 -o <num>: open shell in namespace \"ue<num>\""
  echo "$1 -i <num1> <num2>: create inter-namespace communication between \"ue<num1>\" and \"ue<num2>\""
}

prog_name=$(basename $0)

[[ $(id -u) -eq 0 ]] || die "Please run as root"
[[ $# -eq 0 ]] && { usage $prog_name; die "error: no parameters given"; }

while getopts :c:d:i:ehlo: cmd
do
  case "${cmd}" in
    c) create_namespace ${OPTARG};;
    d) delete_namespace ${OPTARG};;
    e) open_namespace; exit;;
    h) usage ${prog_name}; exit;;
    l) list_namespaces;;
    i) shift $((OPTIND-2))
       [[ $# -ge 2 ]] || { usage ${prog_name}; die "error: -i requires two UE IDs"; }
       inter_namespace_communication $1 $2
       shift 2
       OPTIND=1
       ;;
    o) ue_id=${OPTARG}; open_namespace;;
    ?) usage ${prog_name}; die "Invalid option: -${OPTARG}";;
  esac
done

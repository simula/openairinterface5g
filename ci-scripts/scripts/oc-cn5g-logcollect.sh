#!/bin/bash

set -e

function die() { echo $@; exit 1; }
[ $# -eq 3 ] || die "usage: $0 <path-to-dir> <namespace> <dir>"

OC_DIR=${1}
OC_NS=${2}
DIR=${3}

cat ${OC_DIR}/oc-password | oc login -u oaicicd --server https://api.oai.cs.eurecom.fr:6443 > /dev/null
oc project ${OC_NS} > /dev/null
oc describe pod > ${DIR}/describe-pods-post-test.log
oc get pods.metrics.k8s &> ${DIR}/nf-resource-consumption.log
for p in nrf amf smf upf ausf udm udr; do
  oc logs -l app.kubernetes.io/name=oai-${p} -c ${p} --tail=-1 > ${DIR}/oai-${p}.log
done
oc logs -l app.kubernetes.io/name=mysql --tail=-1 > ${DIR}/mysql.log
oc logs -l app.kubernetes.io/name=oai-traffic-server --tail=-1 > ${DIR}/oai-traffic-server.log
oc logout > /dev/null

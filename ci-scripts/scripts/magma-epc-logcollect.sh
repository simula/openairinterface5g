#!/bin/bash

function die() { echo "${@}"; exit 1; }

[ $# -eq 1 ] || die "usage: $0 <logdir>"

DIR=${1}

docker logs prod-oai-hss &> ${DIR}/oai-hss.log
docker logs prod-magma-mme &> ${DIR}/magme-mme.log
docker logs prod-oai-spgwc &> ${DIR}/oai-spgwc.log
docker logs prod-oai-spgwu-tiny &> ${DIR}/oai-spgwu.log

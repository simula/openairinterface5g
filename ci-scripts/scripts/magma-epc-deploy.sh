#!/bin/bash

function die() { echo "${@}"; exit 1; }

[ $# -eq 1 ] || die "usage: $0 <path>"

DOCKERFILE="${1}"

function wait_for() {
  local timeout=${1}
  local condition=${2}
  echo "wait up to ${timeout} seconds for condition \"${condition}\""
  while [ $timeout -gt 0 ]; do
    eval ${condition} && return
    sleep 1
    let timeout=$timeout-1
  done
  die "ERROR: timed out waiting for condition \"${condition}\""
}

# start database
docker compose -f ${DOCKERFILE} up -d db_init cassandra
wait_for 30 "docker logs prod-db-init 2> /dev/null | grep OK"

# start EPC
docker compose -f ${DOCKERFILE} up -d oai_hss redis magma_mme oai_spgwc oai_spgwu trf_gen
wait_for 20 "docker inspect --format='{{.State.Health.Status}}' prod-cassandra prod-oai-hss prod-magma-mme prod-oai-spgwc prod-oai-spgwu-tiny prod-redis prod-trf-gen | grep healthy | wc -l | grep 7"
echo "deployment OK"

#!/bin/bash

function die() { echo $@; exit 1; }
[ $# -ge 1 ] || die "usage: $0 <directory> [ctest-options]"

IMAGE=ran-physimtests:ci-temp
CONTAINER=ran-physimtests
function cleanup-docker() {
  docker stop ${CONTAINER}
  docker rm ${CONTAINER}
  docker rmi ${IMAGE}
  docker volume prune --force
}
trap cleanup-docker EXIT

set -x
DIR=$1
RESULT_DIR=${DIR}/
shift
CTEST_OPT=$@

# build physims
docker build --progress=plain --tag ${IMAGE} --file ${DIR}/ci-scripts/docker/Dockerfile.physim.ubuntu ${DIR} &>> ${RESULT_DIR}/physim_log.txt
if [ $? -ne 0 ]; then
  echo "build of physims failed"
  exit 1
fi

# get a JSON description of all tests to run
docker run -a STDOUT --workdir /oai-ran/build/ --env LD_LIBRARY_PATH=/oai-ran/build/ --rm --name ${CONTAINER} ${IMAGE} ctest ${CTEST_OPT} --show-only=json-v1 &> ${RESULT_DIR}/desc-tests.json
JSON_RES=$?

# run the actual tests: we don't suppy --rm as we have to copy the files
# similar to unit tests, we can't mount the file where we write physims-5g-run.xml to
# as it would write a file as root, but this script is run as a normal user
docker run -a STDOUT --workdir /oai-ran/build/ --env LD_LIBRARY_PATH=/oai-ran/build/ --name ${CONTAINER} ${IMAGE} ctest ${CTEST_OPT} --output-junit results-run.xml --test-output-size-passed 100000 --test-output-size-failed 100000 &>> ${RESULT_DIR}/physim_log.txt
RUN_RES=$?
docker cp ${CONTAINER}:/oai-ran/build/results-run.xml ${RESULT_DIR}/
docker cp ${CONTAINER}:/oai-ran/build/Testing/Temporary/LastTestsFailed.log ${RESULT_DIR}/
docker cp ${CONTAINER}:/oai-ran/build/Testing/Temporary/LastTest.log ${RESULT_DIR}/

# if both were successful, return 0
# TODO not sure
#[[ $JSON_RES -eq 0 && $RUN_RES -eq 0 ]] && exit 0

exit 0

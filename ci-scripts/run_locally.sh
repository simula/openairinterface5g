#!/bin/bash

set -e
SHORT_COMMIT_SHA=$(git rev-parse --short=8 HEAD)
COMMIT_SHA=$(git rev-parse HEAD)
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
REPO_PATH=$(dirname $(realpath $0))/../
TESTCASE=$1

if [ $# -eq 0 ]
  then
    echo "Provide a testcase as an argument"
    exit 1
fi

# The script assumes you've build the following images:
#
# docker build . -f docker/Dockerfile.gNB.ubuntu -t oai-gnb
# docker build . -f docker/Dockerfile.nr-cuup.ubuntu -t oai-nr-cuup
# docker build . -f docker/Dockerfile.nrUE.ubuntu -t oai-nr-ue
#
# The images above depend on the following images:
#
# docker build . -f docker/Dockerfile.build.ubuntu -t ran-build
# docker build . -f docker/Dockerfile.base.ubuntu -t ran-base

docker tag oai-nr-ue oai-ci/oai-nr-ue:develop-${SHORT_COMMIT_SHA}
docker tag oai-gnb oai-ci/oai-gnb:develop-${SHORT_COMMIT_SHA}
docker tag oai-nr-cuup oai-ci/oai-nr-cuup:develop-${SHORT_COMMIT_SHA}

python3 main.py --mode=InitiateHtml --ranRepository=NONE --ranBranch=${CURRENT_BRANCH} \
    --ranCommitID=${COMMIT_SHA} --ranAllowMerge=false \
    --ranTargetBranch=NONE \
    --XMLTestFile=xml_files/${TESTCASE} --local

python3 main.py --mode=TesteNB --ranRepository=NONE --ranBranch=${CURRENT_BRANCH} \
    --ranCommitID=${COMMIT_SHA} --ranAllowMerge=false \
    --ranTargetBranch=NONE \
    --eNBSourceCodePath=${REPO_PATH} \
    --XMLTestFile=${TESTCASE} --local
RET=$?

python3 main.py --mode=FinalizeHtml --local

exit ${RET}

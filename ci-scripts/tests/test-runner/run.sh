#/bin/bash

branch=$(git rev-parse --abbrev-ref HEAD)
commit=$(git rev-parse HEAD)
file=../../test_results.html
rm -f ${file}

cd ../../
python3 main.py \
  --mode=InitiateHtml \
  --ranRepository=https://gitlab.eurecom.fr/oai/openairinterface5g.git \
  --ranBranch=${branch} \
  --ranCommitID=${commit} \
  --ranAllowMerge=true \
  --ranTargetBranch=develop \
  --XMLTestFile=tests/test-runner/test.xml

python3 main.py \
  --mode=TesteNB \
  --ranRepository=https://gitlab.eurecom.fr/oai/openairinterface5g.git \
  --ranBranch=${branch} \
  --ranCommitID=${commit} \
  --ranAllowMerge=true \
  --ranTargetBranch=develop \
  --eNBIPAddress=localhost \
  --eNBUserName=NONE \
  --eNBPassword=NONE \
  --eNBSourceCodePath=NONE \
  --UEIPAddress=localhost \
  --UEUserName=NONE \
  --UEPassword=NONE \
  --UESourceCodePath=NONE \
  --EPCIPAddress=localhost \
  --EPCType=OAI \
  --EPCUserName=NONE \
  --EPCPassword=NONE \
  --EPCSourceCodePath=NONE \
  --XMLTestFile=tests/test-runner/test.xml

python3 main.py \
  --mode=FinalizeHtml \
  --finalStatus=true
cd -

if [ -f ${file} ]; then
  echo "results are in ${file}"
else
  echo "ERROR: no results file created"
fi

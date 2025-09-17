#!/bin/bash
#/*
# * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
# * contributor license agreements.  See the NOTICE file distributed with
# * this work for additional information regarding copyright ownership.
# * The OpenAirInterface Software Alliance licenses this file to You under
# * the OAI Public License, Version 1.1  (the "License"); you may not use this file
# * except in compliance with the License.
# * You may obtain a copy of the License at
# *
# *      http://www.openairinterface.org/?page_id=698
# *
# * Unless required by applicable law or agreed to in writing, software
# * distributed under the License is distributed on an "AS IS" BASIS,
# * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# * See the License for the specific language governing permissions and
# * limitations under the License.
# *-------------------------------------------------------------------------------
# * For more information about the OpenAirInterface (OAI) Software Alliance:
# *      contact@openairinterface.org
# */

set -eu

AWX_API_URL=https://10.100.1.253
AWX_JOB_ID=16
AWX_API_LAUNCH_PATH=/api/v2/job_templates/${AWX_JOB_ID}/launch/

COL_USER=$1
COL_PASS=$2
JENKINS_JOB_ID=$3
GIT_REPOSITORY=$4
GIT_BRANCH=$5
COLOSSEUM_RF_SCENARIO=$6
JENKINS_JOB_URL=$7

# assemble data to send
CURL_DATA=$(cat <<-END | jq -c .
{
    "extra_vars":
    {
        "oai_repo": "${GIT_REPOSITORY}",
        "oai_branch": "${GIT_BRANCH}",
        "colosseum_rf_scenario": "${COLOSSEUM_RF_SCENARIO}",
        "jenkins_job_id": "${JENKINS_JOB_ID}",
        "jenkins_job_url": "${JENKINS_JOB_URL}"
    }
}
END
)

# launch job
curl -s -f -k -u ${COL_USER}:${COL_PASS} -X POST -H "Content-Type: application/json" -d ${CURL_DATA} ${AWX_API_URL}${AWX_API_LAUNCH_PATH} > launch.json

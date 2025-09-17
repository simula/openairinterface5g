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
AWX_JOB_EVENT_QUERY=job_events/?search=results_url

COL_USER=$1
COL_PASS=$2

AWX_API_JOB_PATH=$(jq -r '.url' launch.json)

# get result url and download test results
curl -s -f -k -u ${COL_USER}:${COL_PASS} -X GET ${AWX_API_URL}${AWX_API_JOB_PATH}${AWX_JOB_EVENT_QUERY} > result.json

set -x
RESULT=$(jq -r '.results[0].event_data.res.ansible_facts.results_url' result.json)
wget -q -O - ${RESULT} > results.tar.xz

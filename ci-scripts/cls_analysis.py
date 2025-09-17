#/*
# * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
# * contributor license agreements.  See the NOTICE file distributed with
# * this work for additional information regarding copyright ownership.
# * The OpenAirInterface Software Alliance licenses this file to You under
# * the OAI Public License, Version 1.1  (the "License"); you may not use this file
# * except in compliance with the License.
# * You may obtain a copy of the License at
# *
# *	  http://www.openairinterface.org/?page_id=698
# *
# * Unless required by applicable law or agreed to in writing, software
# * distributed under the License is distributed on an "AS IS" BASIS,
# * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# * See the License for the specific language governing permissions and
# * limitations under the License.
# *-------------------------------------------------------------------------------
# * For more information about the OpenAirInterface (OAI) Software Alliance:
# *	  contact@openairinterface.org
# */
#---------------------------------------------------------------------

import re
import os

import xml.etree.ElementTree as ET
import json

class Analysis():

	def _get_test_description(properties):
		env_vars = None
		for p in properties:
			if p["name"] == "ENVIRONMENT":
				env_vars = p["value"] # save for later if no custom property
			if p["name"] == "TEST_DESCRIPTION":
				return p["value"]
		# if we came till here, it means there is no custom test property
		# saved in JSON.  See if we have a description in environment variables
		if not env_vars:
			return "<none>"
		for ev in env_vars:
			name, value = ev.split("=", 1)
			if name == "TEST_DESCRIPTION":
				return value
		return "<none>"

	def analyze_physim(result_junit, details_json, logPath):
		try:
			tree = ET.parse(result_junit)
			root = tree.getroot()
			nb_tests = int(root.attrib["tests"])
			nb_failed = int(root.attrib["failures"])
		except ET.ParseError as e:
			return False, False, f'Could not parse XML log file {result_junit}: {e}'
		except FileNotFoundError as e:
			return False, False, f'JUnit XML log file {result_junit} not found: {e}'
		except Exception as e:
			return False, False, f'While parsing JUnit XML log file: exception: {e}'

		try:
			with open(details_json) as f:
				j = json.load(f)
			# prepare JSON for easier access of strings
			json_test_desc = {}
			for e in j["tests"]:
				json_test_desc[e["name"]] = e

		except json.JSONDecodeError as e:
			return False, False, f'Could not decode JSON log file {details_json}: {e}'
		except FileNotFoundError as e:
			return False, False, f'Physim JSON log file {details_json} not found: {e}'
		except Exception as e:
			return False, False, f'While parsing physim JSON log file: exception: {e}'

		test_result = {}
		for test in root: # for each test
			test_name = test.attrib["name"]
			test_exec = json_test_desc[test_name]["properties"][1]["value"][0]
			desc = Analysis._get_test_description(json_test_desc[test_name]["properties"])
			# get runtime and checks
			test_check = test.attrib["status"] == "run"
			time = round(float(test.attrib["time"]), 1)
			time_check = time < 150
			output = test.findtext("system-out")
			output_check = "exceeds the threshold" not in output
			# collect logs
			log_dir = f'{logPath}/{test_exec}'
			os.makedirs(log_dir, exist_ok=True)
			with open(f'{log_dir}/{test_name}.log', 'w') as f:
				f.write(output)
			# prepare result and info
			resultstr = 'PASS' if (test_check and time_check and output_check) else 'FAIL'
			info = f"{test_name}.log: test {resultstr}"
			for l in output.splitlines():
				if l.startswith("CHECK "):
					info += f"\n{l}"
			if test_check:
				if not output_check:
					info += "\nTest log exceeds maximal allowed length 100 kB"
				if not time_check:
					info += "\nTest exceeds 150s"
				if not (time_check and output_check):
					nb_failed += 1 # time threshold/output length error, not counted for by ctest as of now
			test_result[test_name] = [desc, info, resultstr]

		test_summary = {}
		test_summary['Nbtests'] = nb_tests
		test_summary['Nbpass'] =  nb_tests - nb_failed
		test_summary['Nbfail'] =  nb_failed
		return nb_failed == 0, test_summary, test_result

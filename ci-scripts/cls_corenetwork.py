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
#---------------------------------------------------------------------
#
#   Required Python Version
#     Python 3.x
#
#---------------------------------------------------------------------

import logging
import yaml
import re

import cls_cmd
from cls_ci_helper import archiveArtifact

def listify(s):
	if s is None:
		return None
	if isinstance(s, list):
		return [str(x) for x in s]
	return [str(s)]

class CoreNetwork:

	def __init__(self, cn_name, node=None, d=None, filename="ci_infra.yaml"):
		with open(filename, "r") as f:
			all_cns = yaml.load(f, Loader=yaml.FullLoader) # TODO why full?
		c = all_cns.get(cn_name)
		if c is None:
			raise Exception(f'no such core network name "{cn_name}" in "{filename}"')
		self._cn_name = cn_name
		self._host = c.get('Host').strip()
		if self._host == "%%current_host%%":
			if node is None:
				raise Exception(f"core network {cn_name} requires node, but none provided (cannot replace %%current_host%%)")
			self._host = node
		if d is not None:
			raise Exception("directory handling not implemented")
		self._deploy = listify(c.get('Deploy'))
		self._undeploy = listify(c.get('Undeploy'))
		self._logCollect = listify(c.get('LogCollect'))
		# check if at least one command contains "%%log_dir%%"
		if self._logCollect and not any(filter(lambda s: "%%log_dir%%" in s, self._logCollect)):
			raise ValueError(f"(At least one) LogCollect expression for {cn_name} must contain \"%%log_dir%%\"")
		self._getNetwork = listify(c.get('NetworkScript'))
		self._cmd_prefix = c.get('CmdPrefix')
		iperf = c.get('RunIperf3Server')
		self._run_iperf = bool(iperf if iperf is not None else True)
		logging.info(f'initialized core {self} from {filename}')

	def __str__(self):
		return f"{self._cn_name}@{self._host}"

	def __repr__(self):
		return self.__str__()

	def _exec_script(host, line, silent):
		# take off "!", split in words
		words = line[1:].strip().split(" ")
		script_name = words[0]
		options = " ".join(words[1:])
		with cls_cmd.getConnection(host) as c:
			ret = c.exec_script(script_name, 300, parameters=options, silent=silent)
		return ret

	def _command(self, cmd_list, must_succeed=False, silent=False):
		succeeded = True
		output = ""
		with cls_cmd.getConnection(self._host) as c:
			for cmd in cmd_list:
				cmd = cmd.strip()
				if cmd.startswith("!"):
					ret = CoreNetwork._exec_script(self._host, cmd, silent=silent)
				else:
					ret = c.run(cmd, silent=silent)
				output += ret.stdout
				if ret.returncode != 0:
					logging.warn(f"cmd \"{cmd}\" returned code {ret.returncode}, stdout {ret.stdout}")
				if ret.returncode != 0 and must_succeed:
					succeeded = False
					break
		return succeeded, output

	def deploy(self):
		# we first undeploy to make sure the core has been stopped
		logging.info(f'undeploy core network {self} before deployment')
		self._command(self._undeploy)
		logging.info(f'deploy core network {self}')
		success, output = self._command(self._deploy, must_succeed=True)
		if not success:
			logging.error(f'failure during deployment of core network {self}:\n{output}')
			return False, output
		logging.info(f'retrieve IP address')
		ip = self.getIP()
		if ip is None:
			logging.error(f'could not retrieve pingable address of core network {self}')
			return False, output
		logging.info(f'deployed core network {self}, pingable IP address {ip}')
		return True, output

	def _collect_logs(self, ctx):
		remote_dir = "/tmp/cn-undeploy-logs"
		with cls_cmd.getConnection(self._host) as c:
			# create a directory for log collection
			c.run(f'rm -rf {remote_dir}')
			ret = c.run(f'mkdir {remote_dir}')
			if ret.returncode != 0:
				logging.error("cannot create directory for log collection")
				return []
			# inform core network implementation where to store logs
			# (remote_dir), and trigger log collection
			log_cmd = [s.replace('%%log_dir%%', remote_dir) for s in self._logCollect]
			self._command(log_cmd)
			# enumerate collected files
			ret = c.run(f'ls {remote_dir}/*')
			if ret.returncode != 0:
				logging.error("cannot enumerate log files")
				return []
			log_files = []
			# copy them to the executor one by one
			for f in ret.stdout.split("\n"):
				name = archiveArtifact(c, ctx, f)
				log_files.append(name)
			c.run(f'rm -rf {remote_dir}')
			return log_files

	def undeploy(self, ctx=None):
		log_files = []
		if ctx is not None:
			log_files = self._collect_logs(ctx)
		else:
			logging.warning("no directory for log collection specified, cannot retrieve core network logs")
		logging.info(f'undeploy core network {self}')
		_, output = self._command(self._undeploy)
		return log_files, output

	def getIP(self):
		success, output = self._command(self._getNetwork)
		result = re.search(r'inet (?P<ip>[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)', output)
		if success and result and result.group('ip'):
			return result.group('ip')
		return None

	def getCmdPrefix(self):
		return self._cmd_prefix or ""

	def getName(self):
		return self._cn_name

	def getHost(self):
		return self._host

	def runIperf3Server(self):
		return self._run_iperf

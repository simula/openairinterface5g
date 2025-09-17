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
#---------------------------------------------------------------------
# Python for CI of OAI-eNB + COTS-UE
#
#   Required Python Version
#     Python 3.x
#
#   Required Python Package
#     pexpect
#---------------------------------------------------------------------

#-----------------------------------------------------------
# Import
#-----------------------------------------------------------
import sys	      # arg
import re	       # reg
import logging
import os
import shutil
import subprocess
import time
import pyshark
import threading
import cls_cmd
from multiprocessing import Process, Lock, SimpleQueue
from zipfile import ZipFile

#-----------------------------------------------------------
# OAI Testing modules
#-----------------------------------------------------------
import cls_cluster as OC
import cls_cmd
import sshconnection as SSH
import helpreadme as HELP
import constants as CONST
import cls_oaicitest

#-----------------------------------------------------------
# Helper functions used here and in other classes
# (e.g., cls_cluster.py)
#-----------------------------------------------------------
IMAGES = ['oai-enb', 'oai-lte-ru', 'oai-lte-ue', 'oai-gnb', 'oai-nr-cuup', 'oai-gnb-aw2s', 'oai-nr-ue', 'oai-gnb-asan', 'oai-nr-ue-asan', 'oai-nr-cuup-asan', 'oai-gnb-aerial', 'oai-gnb-fhi72']

def CreateWorkspace(host, sourcePath, ranRepository, ranCommitID, ranTargetBranch, ranAllowMerge):
	if ranCommitID == '':
		logging.error('need ranCommitID in CreateWorkspace()')
		raise ValueError('Insufficient Parameter in CreateWorkspace(): need ranCommitID')

	script = "scripts/create_workspace.sh"
	options = f"{sourcePath} {ranRepository} {ranCommitID}"
	if ranAllowMerge:
		if ranTargetBranch == '':
			ranTargetBranch = 'develop'
		options += f" {ranTargetBranch}"
	logging.info(f'execute "{script}" with options "{options}" on node {host}')
	ret = cls_cmd.runScript(host, script, 90, options)
	logging.debug(f'"{script}" finished with code {ret.returncode}, output:\n{ret.stdout}')
	return ret.returncode == 0

def CreateTag(ranCommitID, ranBranch, ranAllowMerge):
	shortCommit = ranCommitID[0:8]
	if ranAllowMerge:
		# Allowing contributor to have a name/branchName format
		branchName = ranBranch.replace('/','-')
		tagToUse = f'{branchName}-{shortCommit}'
	else:
		tagToUse = f'develop-{shortCommit}'
	return tagToUse

def CopyLogsToExecutor(cmd, sourcePath, log_name):
	cmd.cd(f'{sourcePath}/cmake_targets')
	cmd.run(f'rm -f {log_name}.zip')
	cmd.run(f'mkdir -p {log_name}')
	cmd.run(f'mv log/* {log_name}')
	cmd.run(f'zip -r -qq {log_name}.zip {log_name}')

	# copy zip to executor for analysis
	if (os.path.isfile(f'./{log_name}.zip')):
		os.remove(f'./{log_name}.zip')
	if (os.path.isdir(f'./{log_name}')):
		shutil.rmtree(f'./{log_name}')
	cmd.copyin(f'{sourcePath}/cmake_targets/{log_name}.zip', f'./{log_name}.zip')
	cmd.run(f'rm -f {log_name}.zip')
	ZipFile(f'{log_name}.zip').extractall('.')

def AnalyzeBuildLogs(buildRoot, images, globalStatus):
	collectInfo = {}
	for image in images:
		files = {}
		file_list = [f for f in os.listdir(f'{buildRoot}/{image}') if os.path.isfile(os.path.join(f'{buildRoot}/{image}', f)) and f.endswith('.txt')]
		# Analyze the "sub-logs" of every target image
		for fil in file_list:
			errorandwarnings = {}
			warningsNo = 0
			errorsNo = 0
			with open(f'{buildRoot}/{image}/{fil}', mode='r') as inputfile:
				for line in inputfile:
					result = re.search(' ERROR ', str(line))
					if result is not None:
						errorsNo += 1
					result = re.search(' error:', str(line))
					if result is not None:
						errorsNo += 1
					result = re.search(' WARNING ', str(line))
					if result is not None:
						warningsNo += 1
					result = re.search(' warning:', str(line))
					if result is not None:
						warningsNo += 1
				errorandwarnings['errors'] = errorsNo
				errorandwarnings['warnings'] = warningsNo
				errorandwarnings['status'] = globalStatus
			files[fil] = errorandwarnings
		# Analyze the target image
		if os.path.isfile(f'{buildRoot}/{image}.log'):
			errorandwarnings = {}
			committed = False
			tagged = False
			with open(f'{buildRoot}/{image}.log', mode='r') as inputfile:
				for line in inputfile:
					lineHasTag = re.search(f'Successfully tagged {image}:', str(line)) is not None
					lineHasTag2 = re.search(f'naming to docker.io/library/{image}:', str(line)) is not None
					tagged = tagged or lineHasTag or lineHasTag2
					# the OpenShift Cluster builder prepends image registry URL
					lineHasCommit = re.search(f'COMMIT [a-zA-Z0-9\.:/\-]*{image}', str(line)) is not None
					committed = committed or lineHasCommit
			errorandwarnings['errors'] = 0 if committed or tagged else 1
			errorandwarnings['warnings'] = 0
			errorandwarnings['status'] = committed or tagged
			files['Target Image Creation'] = errorandwarnings
		collectInfo[image] = files
	return collectInfo

def GetCredentials(instance):
    server_id = instance.eNB_serverId[instance.eNB_instance]
    if server_id == '0':
        return (instance.eNBIPAddress, instance.eNBUserName, instance.eNBPassword, instance.eNBSourceCodePath)
    elif server_id == '1':
        return (instance.eNB1IPAddress, instance.eNB1UserName, instance.eNB1Password, instance.eNB1SourceCodePath)
    elif server_id == '2':
        return (instance.eNB2IPAddress, instance.eNB2UserName, instance.eNB2Password, instance.eNB2SourceCodePath)
    else:
        raise Exception ("Only supports maximum of 3 servers")

def GetContainerName(ssh, svcName, file):
	ret = ssh.run(f"docker compose -f {file} config --format json {svcName}  | jq -r '.services.\"{svcName}\".container_name'", silent=True)
	return ret.stdout

def GetImageName(ssh, svcName, file):
	ret = ssh.run(f"docker compose -f {file} config --format json {svcName}  | jq -r '.services.\"{svcName}\".image'", silent=True)
	if ret.returncode != 0:
		return f"cannot retrieve image info for {containerName}: {ret.stdout}"
	else:
		return ret.stdout.strip()

def GetContainerHealth(ssh, containerName):
	if containerName is None:
		return False
	if 'db_init' in containerName or 'db-init' in containerName: # exits with 0, there cannot be healthy
		return True
	time.sleep(5)
	for _ in range(3):
		result = ssh.run(f'docker inspect --format="{{{{.State.Health.Status}}}}" {containerName}', silent=True)
		if result.stdout == 'healthy':
			return True
		time.sleep(10)
	return False

def ExistEnvFilePrint(ssh, wd, prompt='env vars in existing'):
	ret = ssh.run(f'cat {wd}/.env', silent=True)
	if ret.returncode != 0:
		return False
	env_vars = ret.stdout.strip().splitlines()
	logging.info(f'{prompt} {wd}/.env: {env_vars}')
	return True

def WriteEnvFile(ssh, services, wd, tag):
	ret = ssh.run(f'cat {wd}/.env', silent=True)
	registry = "oai-ci" # pull_images() gives us this registry path
	envs = {"REGISTRY":registry, "TAG": tag}
	if ret.returncode == 0: # it exists, we have to update
		# transforms env file to dictionary
		old_envs = {}
		for l in ret.stdout.strip().splitlines():
			var, val = l.split('=', 1)
			old_envs[var] = val.strip('"')
		# will retain the old environment variables
		envs = {**envs, **old_envs}
	for svc in services.split():
		# In some scenarios we have the choice of either pulling normal images
		# or -asan images. We need to detect which kind we did pull.
		fullImageName = GetImageName(ssh, svc, f"{wd}/docker-compose.y*ml")
		image = fullImageName.split("/")[-1].split(":")[0]
		checkimg = f"{registry}/{image}-asan:{tag}"
		ret = ssh.run(f'docker image inspect {checkimg}', reportNonZero=False)
		if ret.returncode == 0:
			logging.info(f"detected pulled image {checkimg}")
			if "oai-gnb" in image: envs["GNB_IMG"] = "oai-gnb-asan"
			elif "oai-nr-ue" in image: envs["NRUE_IMG"] = "oai-nr-ue-asan"
			elif "oai-nr-cuup" in image: envs["NRCUUP_IMG"] = "oai-nr-cuup-asan"
			else: logging.warning("undetected image format {image}, cannot use asan")
	env_string = "\n".join([f"{var}=\"{val}\"" for var,val in envs.items()])
	ssh.run(f'echo -e \'{env_string}\' > {wd}/.env', silent=True)
	ExistEnvFilePrint(ssh, wd, prompt='New env vars in file')

def GetServices(ssh, requested, file):
    if requested == [] or requested is None or requested == "":
        logging.warning('No service name given: starting all services in docker-compose.yml!')
        ret = ssh.run(f'docker compose -f {file} config --services')
        if ret.returncode != 0:
            return ""
        else:
            return ' '.join(ret.stdout.splitlines())
    else:
        return requested

def CopyinContainerLog(ssh, lSourcePath, yaml, containerName, filename):
	remote_filename = f"{lSourcePath}/cmake_targets/log/{filename}"
	ssh.run(f'docker logs {containerName} &> {remote_filename}')
	local_dir = f"{os.getcwd()}/../cmake_targets/log/{yaml}"
	os.system(f'mkdir -p {local_dir}')
	local_filename = f"{local_dir}/{filename}"
	return ssh.copyin(remote_filename, local_filename)

def GetRunningServices(ssh, file):
	ret = ssh.run(f'docker compose -f {file} config --services')
	if ret.returncode != 0:
		return None
	allServices = ret.stdout.splitlines()
	running_services = []
	for s in allServices:
		# outputs the hash if the container is running
		ret = ssh.run(f'docker compose -f {file} ps --all --quiet -- {s}')
		if ret.returncode != 0:
			logging.info(f"service {s}: {ret.stdout}")
		elif ret.stdout == "":
			logging.warning(f"could not retrieve information for service {s}")
		else:
			c = ret.stdout
			logging.debug(f'running service {s} with container id {c}')
			running_services.append((s, c))
	logging.info(f'stopping services: {running_services}')
	return running_services

def CheckLogs(self, yaml, service_name, HTML, RAN):
	logPath = f'{os.getcwd()}/../cmake_targets/log/{yaml}'
	filename = f'{logPath}/{service_name}-{HTML.testCase_id}.log'
	success = True
	if (any(sub in service_name for sub in ['oai_ue','oai-nr-ue','lte_ue'])):
		logging.debug(f'\u001B[1m Analyzing UE logfile {filename} \u001B[0m')
		logStatus = cls_oaicitest.OaiCiTest().AnalyzeLogFile_UE(filename, HTML, RAN)
		opt = f"UE log analysis for service {service_name}"
		# usage of htmlUEFailureMsg/htmleNBFailureMsg is because Analyze log files
		# abuse HTML to store their reports, and we here want to put custom options,
		# which is not possible with CreateHtmlTestRow
		# solution: use HTML templates, where we don't need different HTML write funcs
		if (logStatus < 0):
			HTML.CreateHtmlTestRowQueue(opt, 'KO', [HTML.htmlUEFailureMsg])
			success = False
		else:
			HTML.CreateHtmlTestRowQueue(opt, 'OK', [HTML.htmlUEFailureMsg])
		HTML.htmlUEFailureMsg = ""
	elif service_name == 'nv-cubb':
		msg = 'Undeploy PNF/Nvidia CUBB'
		HTML.CreateHtmlTestRow(msg, 'OK', CONST.ALL_PROCESSES_OK)
	elif (any(sub in service_name for sub in ['enb','rru','rcc','cu','du','gnb'])):
		logging.debug(f'\u001B[1m Analyzing XnB logfile {filename}\u001B[0m')
		logStatus = RAN.AnalyzeLogFile_eNB(filename, HTML, self.ran_checkers)
		opt = f"xNB log analysis for service {service_name}"
		if (logStatus < 0):
			HTML.CreateHtmlTestRowQueue(opt, 'KO', [HTML.htmleNBFailureMsg])
			success = False
		else:
			HTML.CreateHtmlTestRowQueue(opt, 'OK', [HTML.htmleNBFailureMsg])
		HTML.htmleNBFailureMsg = ""
	else:
		logging.info(f'Skipping to analyze log for service name {service_name}')
		HTML.CreateHtmlTestRowQueue(f"service {service_name}", 'OK', ["no analysis function"])
	logging.debug(f"log check: service {service_name} passed analysis {success}")
	return success

#-----------------------------------------------------------
# Class Declaration
#-----------------------------------------------------------
class Containerize():

	def __init__(self):
		
		self.ranRepository = ''
		self.ranBranch = ''
		self.ranAllowMerge = False
		self.ranCommitID = ''
		self.ranTargetBranch = ''
		self.eNBIPAddress = ''
		self.eNBUserName = ''
		self.eNBPassword = ''
		self.eNBSourceCodePath = ''
		self.eNB1IPAddress = ''
		self.eNB1UserName = ''
		self.eNB1Password = ''
		self.eNB1SourceCodePath = ''
		self.eNB2IPAddress = ''
		self.eNB2UserName = ''
		self.eNB2Password = ''
		self.eNB2SourceCodePath = ''
		self.forcedWorkspaceCleanup = False
		self.imageKind = ''
		self.proxyCommit = None
		self.eNB_instance = 0
		self.eNB_serverId = ['', '', '']
		self.yamlPath = ['', '', '']
		self.services = ['', '', '']
		self.deploymentTag = ''
		self.eNB_logFile = ['', '', '']

		self.testCase_id = ''

		self.cli = ''
		self.cliBuildOptions = ''
		self.dockerfileprefix = ''
		self.host = ''

		self.deployedContainers = []
		self.tsharkStarted = False
		self.pingContName = ''
		self.pingOptions = ''
		self.pingLossThreshold = ''
		self.svrContName = ''
		self.svrOptions = ''
		self.cliContName = ''
		self.cliOptions = ''

		self.imageToCopy = ''
		self.registrySvrId = ''
		self.testSvrId = ''
		self.imageToPull = []
		#checkers from xml
		self.ran_checkers={}

#-----------------------------------------------------------
# Container management functions
#-----------------------------------------------------------

	def BuildImage(self, HTML):
		if self.ranRepository == '' or self.ranBranch == '' or self.ranCommitID == '':
			HELP.GenericHelp(CONST.Version)
			sys.exit('Insufficient Parameter')
		if self.eNB_serverId[self.eNB_instance] == '0':
			lIpAddr = self.eNBIPAddress
			lUserName = self.eNBUserName
			lPassWord = self.eNBPassword
			lSourcePath = self.eNBSourceCodePath
		elif self.eNB_serverId[self.eNB_instance] == '1':
			lIpAddr = self.eNB1IPAddress
			lUserName = self.eNB1UserName
			lPassWord = self.eNB1Password
			lSourcePath = self.eNB1SourceCodePath
		elif self.eNB_serverId[self.eNB_instance] == '2':
			lIpAddr = self.eNB2IPAddress
			lUserName = self.eNB2UserName
			lPassWord = self.eNB2Password
			lSourcePath = self.eNB2SourceCodePath
		if lIpAddr == '' or lUserName == '' or lPassWord == '' or lSourcePath == '':
			HELP.GenericHelp(CONST.Version)
			sys.exit('Insufficient Parameter')
		logging.debug('Building on server: ' + lIpAddr)
		cmd = cls_cmd.RemoteCmd(lIpAddr)
	
		# Checking the hostname to get adapted on cli and dockerfileprefixes
		cmd.run('hostnamectl')
		result = re.search('Ubuntu|Red Hat', cmd.getBefore())
		self.host = result.group(0)
		if self.host == 'Ubuntu':
			self.cli = 'docker'
			self.dockerfileprefix = '.ubuntu22'
			self.cliBuildOptions = ''
		elif self.host == 'Red Hat':
			self.cli = 'sudo podman'
			self.dockerfileprefix = '.rhel9'
			self.cliBuildOptions = '--disable-compression'

		# we always build the ran-build image with all targets
		# Creating a tupple with the imageName, the DockerFile prefix pattern, targetName and sanitized option
		imageNames = [('ran-build', 'build', 'ran-build', '')]
		result = re.search('eNB', self.imageKind)
		if result is not None:
			imageNames.append(('oai-enb', 'eNB', 'oai-enb', ''))
		result = re.search('gNB', self.imageKind)
		if result is not None:
			imageNames.append(('oai-gnb', 'gNB', 'oai-gnb', ''))
		result = re.search('all', self.imageKind)
		if result is not None:
			imageNames.append(('oai-enb', 'eNB', 'oai-enb', ''))
			imageNames.append(('oai-gnb', 'gNB', 'oai-gnb', ''))
			imageNames.append(('oai-nr-cuup', 'nr-cuup', 'oai-nr-cuup', ''))
			imageNames.append(('oai-lte-ue', 'lteUE', 'oai-lte-ue', ''))
			imageNames.append(('oai-nr-ue', 'nrUE', 'oai-nr-ue', ''))
			if self.host == 'Red Hat':
				imageNames.append(('oai-physim', 'phySim', 'oai-physim', ''))
			if self.host == 'Ubuntu':
				imageNames.append(('oai-lte-ru', 'lteRU', 'oai-lte-ru', ''))
				imageNames.append(('oai-gnb-aerial', 'gNB.aerial', 'oai-gnb-aerial', ''))
				# Building again the 5G images with Address Sanitizer
				imageNames.append(('ran-build', 'build', 'ran-build-asan', '--build-arg "BUILD_OPTION=--sanitize"'))
				imageNames.append(('oai-gnb', 'gNB', 'oai-gnb-asan', '--build-arg "BUILD_OPTION=--sanitize"'))
				imageNames.append(('oai-nr-ue', 'nrUE', 'oai-nr-ue-asan', '--build-arg "BUILD_OPTION=--sanitize"'))
				imageNames.append(('oai-nr-cuup', 'nr-cuup', 'oai-nr-cuup-asan', '--build-arg "BUILD_OPTION=--sanitize"'))
				imageNames.append(('ran-build-fhi72', 'build.fhi72', 'ran-build-fhi72', ''))
				imageNames.append(('oai-gnb', 'gNB.fhi72', 'oai-gnb-fhi72', ''))
		result = re.search('build_cross_arm64', self.imageKind)
		if result is not None:
			self.dockerfileprefix = '.ubuntu22.cross-arm64'
		
		self.testCase_id = HTML.testCase_id
		cmd.cd(lSourcePath)
		# if asterix, copy the entitlement and subscription manager configurations
		if self.host == 'Red Hat':
			cmd.run('mkdir -p ./etc-pki-entitlement')
			cmd.run('cp /etc/pki/entitlement/*.pem ./etc-pki-entitlement/')

		baseImage = 'ran-base'
		baseTag = 'develop'
		forceBaseImageBuild = False
		imageTag = 'develop'
		if (self.ranAllowMerge):
			imageTag = 'ci-temp'
			if self.ranTargetBranch == 'develop':
				cmd.run(f'git diff HEAD..origin/develop -- cmake_targets/build_oai cmake_targets/tools/build_helper docker/Dockerfile.base{self.dockerfileprefix} | grep --colour=never -i INDEX')
				result = re.search('index', cmd.getBefore())
				if result is not None:
					forceBaseImageBuild = True
					baseTag = 'ci-temp'
			# if the branch name contains integration_20xx_wyy, let rebuild ran-base
			result = re.search('integration_20([0-9]{2})_w([0-9]{2})', self.ranBranch)
			if not forceBaseImageBuild and result is not None:
				forceBaseImageBuild = True
				baseTag = 'ci-temp'
		else:
			forceBaseImageBuild = True

		# Let's remove any previous run artifacts if still there
		cmd.run(f"{self.cli} image prune --force")
		for image,pattern,name,option in imageNames:
			cmd.run(f"{self.cli} image rm {name}:{imageTag}")

		# Build the base image only on Push Events (not on Merge Requests)
		# On when the base image docker file is being modified.
		if forceBaseImageBuild:
			cmd.run(f"{self.cli} image rm {baseImage}:{baseTag}")
			cmd.run(f"{self.cli} build {self.cliBuildOptions} --target {baseImage} --tag {baseImage}:{baseTag} --file docker/Dockerfile.base{self.dockerfileprefix} . &> cmake_targets/log/ran-base.log", timeout=1600)
		# First verify if the base image was properly created.
		ret = cmd.run(f"{self.cli} image inspect --format=\'Size = {{{{.Size}}}} bytes\' {baseImage}:{baseTag}")
		allImagesSize = {}
		if ret.returncode != 0:
			logging.error('\u001B[1m Could not build properly ran-base\u001B[0m')
			# Recover the name of the failed container?
			cmd.run(f"{self.cli} ps --quiet --filter \"status=exited\" -n1 | xargs --no-run-if-empty {self.cli} rm -f")
			cmd.run(f"{self.cli} image prune --force")
			cmd.close()
			logging.error('\u001B[1m Building OAI Images Failed\u001B[0m')
			HTML.CreateHtmlTestRow(self.imageKind, 'KO', CONST.ALL_PROCESSES_OK)
			HTML.CreateHtmlTabFooter(False)
			return False
		else:
			result = re.search('Size *= *(?P<size>[0-9\-]+) *bytes', cmd.getBefore())
			if result is not None:
				size = float(result.group("size")) / 1000000
				imageSizeStr = f'{size:.1f}'
				logging.debug(f'\u001B[1m   ran-base size is {imageSizeStr} Mbytes\u001B[0m')
				allImagesSize['ran-base'] = f'{imageSizeStr} Mbytes'
			else:
				logging.debug('ran-base size is unknown')

		# Recover build logs, for the moment only possible when build is successful
		cmd.run(f"{self.cli} create --name test {baseImage}:{baseTag}")
		cmd.run("mkdir -p cmake_targets/log/ran-base")
		cmd.run(f"{self.cli} cp test:/oai-ran/cmake_targets/log/. cmake_targets/log/ran-base")
		cmd.run(f"{self.cli} rm -f test")

		# Build the target image(s)
		status = True
		attemptedImages = ['ran-base']
		for image,pattern,name,option in imageNames:
			attemptedImages += [name]
			# the archived Dockerfiles have "ran-base:latest" as base image
			# we need to update them with proper tag
			cmd.run(f'git checkout -- docker/Dockerfile.{pattern}{self.dockerfileprefix}')
			cmd.run(f'sed -i -e "s#{baseImage}:latest#{baseImage}:{baseTag}#" docker/Dockerfile.{pattern}{self.dockerfileprefix}')
			# target images should use the proper ran-build image
			if image != 'ran-build' and "-asan" in name:
				cmd.run(f'sed -i -e "s#ran-build:latest#ran-build-asan:{imageTag}#" docker/Dockerfile.{pattern}{self.dockerfileprefix}')
			elif "fhi72" in name:
				cmd.run(f'sed -i -e "s#ran-build-fhi72:latest#ran-build-fhi72:{imageTag}#" docker/Dockerfile.{pattern}{self.dockerfileprefix}')
			elif image != 'ran-build':
				cmd.run(f'sed -i -e "s#ran-build:latest#ran-build:{imageTag}#" docker/Dockerfile.{pattern}{self.dockerfileprefix}')
			if image == 'oai-gnb-aerial':
				cmd.run('cp -f /opt/nvidia-ipc/nvipc_src.2024.05.23.tar.gz .')
			ret = cmd.run(f'{self.cli} build {self.cliBuildOptions} --target {image} --tag {name}:{imageTag} --file docker/Dockerfile.{pattern}{self.dockerfileprefix} {option} . > cmake_targets/log/{name}.log 2>&1', timeout=1200)
			if image == 'oai-gnb-aerial':
				cmd.run('rm -f nvipc_src.2024.05.23.tar.gz')
			if image == 'ran-build' and ret.returncode == 0:
				cmd.run(f"docker run --name test-log -d {name}:{imageTag} /bin/true")
				cmd.run(f"docker cp test-log:/oai-ran/cmake_targets/log/ cmake_targets/log/{name}/")
				cmd.run(f"docker rm -f test-log")
			else:
				cmd.run(f"mkdir -p cmake_targets/log/{name}")
			# check the status of the build
			ret = cmd.run(f"{self.cli} image inspect --format=\'Size = {{{{.Size}}}} bytes\' {name}:{imageTag}")
			if ret.returncode != 0:
				logging.error('\u001B[1m Could not build properly ' + name + '\u001B[0m')
				status = False
				# Here we should check if the last container corresponds to a failed command and destroy it
				cmd.run(f"{self.cli} ps --quiet --filter \"status=exited\" -n1 | xargs --no-run-if-empty {self.cli} rm -f")
				allImagesSize[name] = 'N/A -- Build Failed'
				break
			else:
				result = re.search('Size *= *(?P<size>[0-9\-]+) *bytes', cmd.getBefore())
				if result is not None:
					size = float(result.group("size")) / 1000000 # convert to MB
					imageSizeStr = f'{size:.1f}'
					logging.debug(f'\u001B[1m   {name} size is {imageSizeStr} Mbytes\u001B[0m')
					allImagesSize[name] = f'{imageSizeStr} Mbytes'
				else:
					logging.debug(f'{name} size is unknown')
					allImagesSize[name] = 'unknown'
			# Now pruning dangling images in between target builds
			cmd.run(f"{self.cli} image prune --force")

		# Remove all intermediate build images and clean up
		cmd.run(f"{self.cli} image rm ran-build:{imageTag} ran-build-asan:{imageTag} ran-build-fhi72:{imageTag} || true")
		cmd.run(f"{self.cli} volume prune --force")

		# Remove some cached artifacts to prevent out of diskspace problem
		logging.debug(cmd.run("df -h").stdout)
		logging.debug(cmd.run("docker system df").stdout)
		cmd.run(f"{self.cli} buildx prune --filter until=1h --force")
		logging.debug(cmd.run("df -h").stdout)
		logging.debug(cmd.run("docker system df").stdout)

		# create a zip with all logs
		build_log_name = f'build_log_{self.testCase_id}'
		CopyLogsToExecutor(cmd, lSourcePath, build_log_name)
		cmd.close()

		# Analyze the logs
		collectInfo = AnalyzeBuildLogs(build_log_name, attemptedImages, status)
		
		if status:
			logging.info('\u001B[1m Building OAI Image(s) Pass\u001B[0m')
			HTML.CreateHtmlTestRow(self.imageKind, 'OK', CONST.ALL_PROCESSES_OK)
			HTML.CreateHtmlNextTabHeaderTestRow(collectInfo, allImagesSize)
			return True
		else:
			logging.error('\u001B[1m Building OAI Images Failed\u001B[0m')
			HTML.CreateHtmlTestRow(self.imageKind, 'KO', CONST.ALL_PROCESSES_OK)
			HTML.CreateHtmlNextTabHeaderTestRow(collectInfo, allImagesSize)
			HTML.CreateHtmlTabFooter(False)
			return False

	def BuildProxy(self, HTML):
		if self.ranRepository == '' or self.ranBranch == '' or self.ranCommitID == '':
			HELP.GenericHelp(CONST.Version)
			sys.exit('Insufficient Parameter')
		if self.eNB_serverId[self.eNB_instance] == '0':
			lIpAddr = self.eNBIPAddress
			lUserName = self.eNBUserName
			lPassWord = self.eNBPassword
			lSourcePath = self.eNBSourceCodePath
		elif self.eNB_serverId[self.eNB_instance] == '1':
			lIpAddr = self.eNB1IPAddress
			lUserName = self.eNB1UserName
			lPassWord = self.eNB1Password
			lSourcePath = self.eNB1SourceCodePath
		elif self.eNB_serverId[self.eNB_instance] == '2':
			lIpAddr = self.eNB2IPAddress
			lUserName = self.eNB2UserName
			lPassWord = self.eNB2Password
			lSourcePath = self.eNB2SourceCodePath
		if lIpAddr == '' or lUserName == '' or lPassWord == '' or lSourcePath == '':
			HELP.GenericHelp(CONST.Version)
			sys.exit('Insufficient Parameter')
		if self.proxyCommit is None:
			HELP.GenericHelp(CONST.Version)
			sys.exit('Insufficient Parameter (need proxyCommit for proxy build)')
		logging.debug('Building on server: ' + lIpAddr)
		mySSH = SSH.SSHConnection()
		mySSH.open(lIpAddr, lUserName, lPassWord)

		# Check that we are on Ubuntu
		mySSH.command('hostnamectl', '\$', 5)
		result = re.search('Ubuntu',  mySSH.getBefore())
		self.host = result.group(0)
		if self.host != 'Ubuntu':
			logging.error('\u001B[1m Can build proxy only on Ubuntu server\u001B[0m')
			mySSH.close()
			sys.exit(1)

		self.cli = 'docker'
		self.cliBuildOptions = ''

		# Workaround for some servers, we need to erase completely the workspace
		if self.forcedWorkspaceCleanup:
			mySSH.command('echo ' + lPassWord + ' | sudo -S rm -Rf ' + lSourcePath, '\$', 15)

		oldRanCommidID = self.ranCommitID
		oldRanRepository = self.ranRepository
		oldRanAllowMerge = self.ranAllowMerge
		oldRanTargetBranch = self.ranTargetBranch
		self.ranCommitID = self.proxyCommit
		self.ranRepository = 'https://github.com/EpiSci/oai-lte-5g-multi-ue-proxy.git'
		self.ranAllowMerge = False
		self.ranTargetBranch = 'master'
		mySSH.command('cd ' +lSourcePath, '\$', 3)
		# to prevent accidentally overwriting data that might be used later
		self.ranCommitID = oldRanCommidID
		self.ranRepository = oldRanRepository
		self.ranAllowMerge = oldRanAllowMerge
		self.ranTargetBranch = oldRanTargetBranch

		# Let's remove any previous run artifacts if still there
		mySSH.command(self.cli + ' image prune --force', '\$', 30)
		# Remove any previous proxy image
		mySSH.command(self.cli + ' image rm oai-lte-multi-ue-proxy:latest || true', '\$', 30)

		tag = self.proxyCommit
		logging.debug('building L2sim proxy image for tag ' + tag)
		# check if the corresponding proxy image with tag exists. If not, build it
		mySSH.command(self.cli + ' image inspect --format=\'Size = {{.Size}} bytes\' proxy:' + tag, '\$', 5)
		buildProxy = mySSH.getBefore().count('o such image') != 0
		if buildProxy:
			mySSH.command(self.cli + ' build ' + self.cliBuildOptions + ' --target oai-lte-multi-ue-proxy --tag proxy:' + tag + ' --file docker/Dockerfile.ubuntu18.04 . > cmake_targets/log/proxy-build.log 2>&1', '\$', 180)
			mySSH.command(self.cli + ' image inspect --format=\'Size = {{.Size}} bytes\' proxy:' + tag, '\$', 5)
			mySSH.command(self.cli + ' image prune --force || true','\$', 15)
			if mySSH.getBefore().count('o such image') != 0:
				logging.error('\u001B[1m Build of L2sim proxy failed\u001B[0m')
				mySSH.close()
				HTML.CreateHtmlTestRow('commit ' + tag, 'KO', CONST.ALL_PROCESSES_OK)
				HTML.CreateHtmlTabFooter(False)
				return False
		else:
			logging.debug('L2sim proxy image for tag ' + tag + ' already exists, skipping build')

		# retag the build images to that we pick it up later
		mySSH.command('docker image tag proxy:' + tag + ' oai-lte-multi-ue-proxy:latest', '\$', 5)

		# no merge: is a push to develop, tag the image so we can push it to the registry
		if not self.ranAllowMerge:
			mySSH.command('docker image tag proxy:' + tag + ' proxy:develop', '\$', 5)

		# we assume that the host on which this is built will also run the proxy. The proxy
		# currently requires the following command, and the docker-compose up mechanism of
		# the CI does not allow to run arbitrary commands. Note that the following actually
		# belongs to the deployment, not the build of the proxy...
		logging.warning('the following command belongs to deployment, but no mechanism exists to exec it there!')
		mySSH.command('sudo ifconfig lo: 127.0.0.2 netmask 255.0.0.0 up', '\$', 5)

		# Analyzing the logs
		if buildProxy:
			self.testCase_id = HTML.testCase_id
			mySSH.command('cd ' + lSourcePath + '/cmake_targets', '\$', 5)
			mySSH.command('mkdir -p proxy_build_log_' + self.testCase_id, '\$', 5)
			mySSH.command('mv log/* ' + 'proxy_build_log_' + self.testCase_id, '\$', 5)
			if (os.path.isfile('./proxy_build_log_' + self.testCase_id + '.zip')):
				os.remove('./proxy_build_log_' + self.testCase_id + '.zip')
			if (os.path.isdir('./proxy_build_log_' + self.testCase_id)):
				shutil.rmtree('./proxy_build_log_' + self.testCase_id)
			mySSH.command('zip -r -qq proxy_build_log_' + self.testCase_id + '.zip proxy_build_log_' + self.testCase_id, '\$', 5)
			mySSH.copyin(lIpAddr, lUserName, lPassWord, lSourcePath + '/cmake_targets/build_log_' + self.testCase_id + '.zip', '.')
			# don't delete such that we might recover the zips
			#mySSH.command('rm -f build_log_' + self.testCase_id + '.zip','\$', 5)

		# we do not analyze the logs (we assume the proxy builds fine at this stage),
		# but need to have the following information to correctly display the HTML
		files = {}
		errorandwarnings = {}
		errorandwarnings['errors'] = 0
		errorandwarnings['warnings'] = 0
		errorandwarnings['status'] = True
		files['Target Image Creation'] = errorandwarnings
		collectInfo = {}
		collectInfo['proxy'] = files
		mySSH.command('docker image inspect --format=\'Size = {{.Size}} bytes\' proxy:' + tag, '\$', 5)
		result = re.search('Size *= *(?P<size>[0-9\-]+) *bytes', mySSH.getBefore())
		# Cleaning any created tmp volume
		mySSH.command(self.cli + ' volume prune --force || true','\$', 15)
		mySSH.close()

		allImagesSize = {}
		if result is not None:
			imageSize = float(result.group('size')) / 1000000
			logging.debug('\u001B[1m   proxy size is ' + ('%.0f' % imageSize) + ' Mbytes\u001B[0m')
			allImagesSize['proxy'] = str(round(imageSize,1)) + ' Mbytes'
			logging.info('\u001B[1m Building L2sim Proxy Image Pass\u001B[0m')
			HTML.CreateHtmlTestRow('commit ' + tag, 'OK', CONST.ALL_PROCESSES_OK)
			HTML.CreateHtmlNextTabHeaderTestRow(collectInfo, allImagesSize)
			return True
		else:
			logging.error('proxy size is unknown')
			allImagesSize['proxy'] = 'unknown'
			logging.error('\u001B[1m Build of L2sim proxy failed\u001B[0m')
			HTML.CreateHtmlTestRow('commit ' + tag, 'KO', CONST.ALL_PROCESSES_OK)
			HTML.CreateHtmlTabFooter(False)
			return False

	def BuildRunTests(self, HTML):
		if self.ranRepository == '' or self.ranBranch == '' or self.ranCommitID == '':
			HELP.GenericHelp(CONST.Version)
			sys.exit('Insufficient Parameter')
		if self.eNB_serverId[self.eNB_instance] == '0':
			lIpAddr = self.eNBIPAddress
			lUserName = self.eNBUserName
			lPassWord = self.eNBPassword
			lSourcePath = self.eNBSourceCodePath
		elif self.eNB_serverId[self.eNB_instance] == '1':
			lIpAddr = self.eNB1IPAddress
			lUserName = self.eNB1UserName
			lPassWord = self.eNB1Password
			lSourcePath = self.eNB1SourceCodePath
		elif self.eNB_serverId[self.eNB_instance] == '2':
			lIpAddr = self.eNB2IPAddress
			lUserName = self.eNB2UserName
			lPassWord = self.eNB2Password
			lSourcePath = self.eNB2SourceCodePath
		if lIpAddr == '' or lUserName == '' or lPassWord == '' or lSourcePath == '':
			HELP.GenericHelp(CONST.Version)
			sys.exit('Insufficient Parameter')
		logging.debug('Building on server: ' + lIpAddr)
		cmd = cls_cmd.RemoteCmd(lIpAddr)
		cmd.cd(lSourcePath)

		ret = cmd.run('hostnamectl')
		result = re.search('Ubuntu', ret.stdout)
		host = result.group(0)
		if host != 'Ubuntu':
			cmd.close()
			raise Exception("Can build unit tests only on Ubuntu server")
		logging.debug('running on Ubuntu as expected')

		if self.forcedWorkspaceCleanup:
			cmd.run(f'sudo -S rm -Rf {lSourcePath}')
		self.testCase_id = HTML.testCase_id
	
		# check that ran-base image exists as we expect it
		baseImage = 'ran-base'
		baseTag = 'develop'
		if self.ranAllowMerge:
			if self.ranTargetBranch == 'develop':
				cmd.run(f'git diff HEAD..origin/develop -- cmake_targets/build_oai cmake_targets/tools/build_helper docker/Dockerfile.base{self.dockerfileprefix} | grep --colour=never -i INDEX')
				result = re.search('index', cmd.getBefore())
				if result is not None:
					baseTag = 'ci-temp'
		ret = cmd.run(f"docker image inspect --format=\'Size = {{{{.Size}}}} bytes\' {baseImage}:{baseTag}")
		if ret.returncode != 0:
			logging.error(f'No {baseImage} image present, cannot build tests')
			HTML.CreateHtmlTestRow(self.imageKind, 'KO', CONST.ALL_PROCESSES_OK)
			HTML.CreateHtmlTabFooter(False)
			return False

		# build ran-unittests image
		dockerfile = "ci-scripts/docker/Dockerfile.unittest.ubuntu22"
		ret = cmd.run(f'docker build --progress=plain --tag ran-unittests:{baseTag} --file {dockerfile} . &> {lSourcePath}/cmake_targets/log/unittest-build.log')
		if ret.returncode != 0:
			build_log_name = f'build_log_{self.testCase_id}'
			CopyLogsToExecutor(cmd, lSourcePath, build_log_name)
			logging.error(f'Cannot build unit tests')
			HTML.CreateHtmlTestRow("Unit test build failed", 'KO', [dockerfile])
			HTML.CreateHtmlTabFooter(False)
			return False

		HTML.CreateHtmlTestRowQueue("Build unit tests", 'OK', [dockerfile])

		# it worked, build and execute tests, and close connection
		ret = cmd.run(f'docker run -a STDOUT --rm ran-unittests:{baseTag} ctest --output-on-failure --no-label-summary -j$(nproc)')
		cmd.run(f'docker rmi ran-unittests:{baseTag}')
		build_log_name = f'build_log_{self.testCase_id}'
		CopyLogsToExecutor(cmd, lSourcePath, build_log_name)
		cmd.close()

		if ret.returncode == 0:
			HTML.CreateHtmlTestRowQueue('Unit tests succeeded', 'OK', [ret.stdout])
			HTML.CreateHtmlTabFooter(True)
			return True
		else:
			HTML.CreateHtmlTestRowQueue('Unit tests failed (see also doc/UnitTests.md)', 'KO', [ret.stdout])
			HTML.CreateHtmlTabFooter(False)
			return False

	def Push_Image_to_Local_Registry(self, HTML):
		if self.registrySvrId == '0':
			lIpAddr = self.eNBIPAddress
			lUserName = self.eNBUserName
			lPassWord = self.eNBPassword
			lSourcePath = self.eNBSourceCodePath
		elif self.registrySvrId == '1':
			lIpAddr = self.eNB1IPAddress
			lUserName = self.eNB1UserName
			lPassWord = self.eNB1Password
			lSourcePath = self.eNB1SourceCodePath
		elif self.registrySvrId == '2':
			lIpAddr = self.eNB2IPAddress
			lUserName = self.eNB2UserName
			lPassWord = self.eNB2Password
			lSourcePath = self.eNB2SourceCodePath
		if lIpAddr == '' or lUserName == '' or lPassWord == '' or lSourcePath == '':
			HELP.GenericHelp(CONST.Version)
			sys.exit('Insufficient Parameter')
		logging.debug('Pushing images from server: ' + lIpAddr)
		mySSH = SSH.SSHConnection()
		mySSH.open(lIpAddr, lUserName, lPassWord)
		imagePrefix = 'porcepix.sboai.cs.eurecom.fr'
		mySSH.command(f'docker login -u oaicicd -p oaicicd {imagePrefix}', '\$', 5)
		if re.search('Login Succeeded', mySSH.getBefore()) is None:
			msg = 'Could not log into local registry'
			logging.error(msg)
			mySSH.close()
			HTML.CreateHtmlTestRow(msg, 'KO', CONST.ALL_PROCESSES_OK)
			return False

		orgTag = 'develop'
		if self.ranAllowMerge:
			orgTag = 'ci-temp'
		for image in IMAGES:
			tagToUse = CreateTag(self.ranCommitID, self.ranBranch, self.ranAllowMerge)
			imageTag = f"{image}:{tagToUse}"
			mySSH.command(f'docker image tag {image}:{orgTag} {imagePrefix}/{imageTag}', '\$', 5)
			if re.search('Error response from daemon: No such image:', mySSH.getBefore()) is not None:
				continue
			mySSH.command(f'docker push {imagePrefix}/{imageTag}', '\$', 120)
			if re.search(': digest:', mySSH.getBefore()) is None:
				logging.debug(mySSH.getBefore())
				msg = f'Could not push {image} to local registry : {imageTag}'
				logging.error(msg)
				mySSH.close()
				HTML.CreateHtmlTestRow(msg, 'KO', CONST.ALL_PROCESSES_OK)
				return False
			mySSH.command(f'docker rmi {imagePrefix}/{imageTag} {image}:{orgTag}', '\$', 30)

		mySSH.command(f'docker logout {imagePrefix}', '\$', 5)
		if re.search('Removing login credentials', mySSH.getBefore()) is None:
			msg = 'Could not log off from local registry'
			logging.error(msg)
			mySSH.close()
			HTML.CreateHtmlTestRow(msg, 'KO', CONST.ALL_PROCESSES_OK)
			return False

		mySSH.close()
		HTML.CreateHtmlTestRow('N/A', 'OK', CONST.ALL_PROCESSES_OK)
		return True

	def Pull_Image_from_Local_Registry(self, HTML):
		# This method can be called either onto a remote server (different from python executor)
		# or directly on the python executor (ie lIpAddr == 'none')
		if self.testSvrId == '0':
			lIpAddr = self.eNBIPAddress
			lUserName = self.eNBUserName
			lPassWord = self.eNBPassword
			lSourcePath = self.eNBSourceCodePath
		elif self.testSvrId == '1':
			lIpAddr = self.eNB1IPAddress
			lUserName = self.eNB1UserName
			lPassWord = self.eNB1Password
			lSourcePath = self.eNB1SourceCodePath
		elif self.testSvrId == '2':
			lIpAddr = self.eNB2IPAddress
			lUserName = self.eNB2UserName
			lPassWord = self.eNB2Password
			lSourcePath = self.eNB2SourceCodePath
		if lIpAddr == '' or lUserName == '' or lPassWord == '' or lSourcePath == '':
			HELP.GenericHelp(CONST.Version)
			sys.exit('Insufficient Parameter')
		logging.debug('\u001B[1m Pulling image(s) on server: ' + lIpAddr + '\u001B[0m')
		myCmd = cls_cmd.getConnection(lIpAddr)
		imagePrefix = 'porcepix.sboai.cs.eurecom.fr'
		response = myCmd.run(f'docker login -u oaicicd -p oaicicd {imagePrefix}')
		if response.returncode != 0:
			msg = 'Could not log into local registry'
			logging.error(msg)
			myCmd.close()
			HTML.CreateHtmlTestRow(msg, 'KO', CONST.ALL_PROCESSES_OK)
			return False
		pulled_images = []
		for image in self.imageToPull:
			tagToUse = CreateTag(self.ranCommitID, self.ranBranch, self.ranAllowMerge)
			imageTag = f"{image}:{tagToUse}"
			cmd = f'docker pull {imagePrefix}/{imageTag}'
			response = myCmd.run(cmd, timeout=120)
			if response.returncode != 0:
				logging.debug(response)
				msg = f'Could not pull {image} from local registry : {imageTag}'
				logging.error(msg)
				myCmd.close()
				HTML.CreateHtmlTestRow('msg', 'KO', CONST.ALL_PROCESSES_OK)
				return False
			myCmd.run(f'docker tag {imagePrefix}/{imageTag} oai-ci/{imageTag}')
			myCmd.run(f'docker rmi {imagePrefix}/{imageTag}')
			pulled_images += [f"oai-ci/{imageTag}"]
		response = myCmd.run(f'docker logout {imagePrefix}')
		if response.returncode != 0:
			msg = 'Could not log off from local registry'
			logging.error(msg)
			myCmd.close()
			HTML.CreateHtmlTestRow(msg, 'KO', CONST.ALL_PROCESSES_OK)
			return False
		myCmd.close()
		msg = "Pulled Images:\n" + '\n'.join(pulled_images)
		HTML.CreateHtmlTestRowQueue('N/A', 'OK', [msg])
		return True

	def Clean_Test_Server_Images(self, HTML):
		# This method can be called either onto a remote server (different from python executor)
		# or directly on the python executor (ie lIpAddr == 'none')
		if self.testSvrId == '0':
			lIpAddr = self.eNBIPAddress
			lUserName = self.eNBUserName
			lPassWord = self.eNBPassword
			lSourcePath = self.eNBSourceCodePath
		elif self.testSvrId == '1':
			lIpAddr = self.eNB1IPAddress
			lUserName = self.eNB1UserName
			lPassWord = self.eNB1Password
			lSourcePath = self.eNB1SourceCodePath
		elif self.testSvrId == '2':
			lIpAddr = self.eNB2IPAddress
			lUserName = self.eNB2UserName
			lPassWord = self.eNB2Password
			lSourcePath = self.eNB2SourceCodePath
		if lIpAddr == '' or lUserName == '' or lPassWord == '' or lSourcePath == '':
			HELP.GenericHelp(CONST.Version)
			sys.exit('Insufficient Parameter')
		if lIpAddr != 'none':
			logging.debug('Removing test images from server: ' + lIpAddr)
			myCmd = cls_cmd.RemoteCmd(lIpAddr)
		else:
			logging.debug('Removing test images locally')
			myCmd = cls_cmd.LocalCmd()

		for image in IMAGES:
			tag = CreateTag(self.ranCommitID, self.ranBranch, self.ranAllowMerge)
			imageTag = f"{image}:{tag}"
			cmd = f'docker rmi oai-ci/{imageTag}'
			myCmd.run(cmd, reportNonZero=False)

		myCmd.close()
		HTML.CreateHtmlTestRow('N/A', 'OK', CONST.ALL_PROCESSES_OK)
		return True

	def Create_Workspace(self,HTML):
		if self.eNB_serverId[self.eNB_instance] == '0':
			lIpAddr = self.eNBIPAddress
			lUserName = self.eNBUserName
			lPassWord = self.eNBPassword
			lSourcePath = self.eNBSourceCodePath
		elif self.eNB_serverId[self.eNB_instance] == '1':
			lIpAddr = self.eNB1IPAddress
			lUserName = self.eNB1UserName
			lPassWord = self.eNB1Password
			lSourcePath = self.eNB1SourceCodePath
		elif self.eNB_serverId[self.eNB_instance] == '2':
			lIpAddr = self.eNB2IPAddress
			lUserName = self.eNB2UserName
			lPassWord = self.eNB2Password
			lSourcePath = self.eNB2SourceCodePath
		if lIpAddr == '' or lUserName == '' or lPassWord == '' or lSourcePath == '':
			HELP.GenericHelp(CONST.Version)
			sys.exit('Insufficient Parameter')
		
		success = CreateWorkspace(lIpAddr, lSourcePath, self.ranRepository, self.ranCommitID, self.ranTargetBranch, self.ranAllowMerge)
		if success:
			HTML.CreateHtmlTestRowQueue('N/A', 'OK', [f"created workspace {lSourcePath}"])
		else:
			HTML.CreateHtmlTestRowQueue('N/A', 'KO', ["cannot create workspace"])
		return success

	def DeployObject(self, HTML):
		lIpAddr, lUserName, lPassWord, lSourcePath = GetCredentials(self)
		if lIpAddr == '' or lUserName == '' or lPassWord == '' or lSourcePath == '':
			HELP.GenericHelp(CONST.Version)
			sys.exit('Insufficient Parameter')
		logging.debug('\u001B[1m Deploying OAI Object on server: ' + lIpAddr + '\u001B[0m')
		yaml = self.yamlPath[self.eNB_instance].strip('/')
		wd = f'{lSourcePath}/{yaml}'

		with cls_cmd.getConnection(lIpAddr) as ssh:
			services = GetServices(ssh, self.services[self.eNB_instance], f"{wd}/docker-compose.y*ml")
			if services == [] or services == ' ' or services == None:
				msg = "Cannot determine services to start"
				logging.error(msg)
				HTML.CreateHtmlTestRowQueue('N/A', 'KO', [msg])
				return False

			ExistEnvFilePrint(ssh, wd)
			WriteEnvFile(ssh, services, wd, self.deploymentTag)

			logging.info(f"will start services {services}")
			status = ssh.run(f'docker compose -f {wd}/docker-compose.y*ml up -d -- {services}')
			if status.returncode != 0:
				msg = f"cannot deploy services {services}: {status.stdout}"
				logging.error(msg)
				HTML.CreateHtmlTestRowQueue('N/A', 'KO', [msg])
				return False

			imagesInfo = []
			fstatus = True
			for svc in services.split():
				containerName = GetContainerName(ssh, svc, f"{wd}/docker-compose.y*ml")
				healthy = GetContainerHealth(ssh, containerName)
				if not healthy:
					tgtlogfile = f'{svc}-{HTML.testCase_id}.log'
					logging.warning(f"Deployment Failed: Trying to copy container logs to {tgtlogfile}")
					yaml_dir = yaml.split('/')[-1]
					CopyinContainerLog(ssh, lSourcePath, yaml_dir, containerName, tgtlogfile)
					imagesInfo += [f"Failed to deploy: service {svc}"]
					fstatus = False
				else:
					image = GetImageName(ssh, svc, f"{wd}/docker-compose.y*ml")
					logging.info(f"service {svc} healthy, container {containerName}, image {image}")
					imagesInfo += [f"service {svc} healthy, image {image}"]
		if fstatus:
			HTML.CreateHtmlTestRowQueue('N/A', 'OK', ['\n'.join(imagesInfo)])
		else:
			HTML.CreateHtmlTestRowQueue('N/A', 'KO', ['\n'.join(imagesInfo)])
		return fstatus

	def UndeployObject(self, HTML, RAN):
		lIpAddr, lUserName, lPassWord, lSourcePath = GetCredentials(self)
		if lIpAddr == '' or lUserName == '' or lPassWord == '' or lSourcePath == '':
			HELP.GenericHelp(CONST.Version)
			sys.exit('Insufficient Parameter')
		logging.debug(f'\u001B[1m Undeploying OAI Object from server: {lIpAddr}\u001B[0m')
		yaml = self.yamlPath[self.eNB_instance].strip('/')
		wd = f'{lSourcePath}/{yaml}'
		yaml_dir = yaml.split('/')[-1]
		with cls_cmd.getConnection(lIpAddr) as ssh:
			ExistEnvFilePrint(ssh, wd)
			services = GetRunningServices(ssh, f"{wd}/docker-compose.y*ml")
			copyin_res = None
			if services is not None:
				all_serv = " ".join([s for s, _ in services])
				ssh.run(f'docker compose -f {wd}/docker-compose.y*ml stop -- {all_serv}')
				copyin_res = all(CopyinContainerLog(ssh, lSourcePath, yaml_dir, c, f'{s}-{HTML.testCase_id}.log') for s, c in services)
			else:
				logging.warning('could not identify services to stop => no log file')
			ssh.run(f'docker compose -f {wd}/docker-compose.y*ml down -v')
			ssh.run(f'rm {wd}/.env')
		if not copyin_res:
			HTML.CreateHtmlTestRowQueue('N/A', 'KO', ['Could not copy logfile(s)'])
			return False
		else:
			log_results = [CheckLogs(self, yaml_dir, s, HTML, RAN) for s, _ in services]
			success = all(log_results)
		if success:
			logging.info('\u001B[1m Undeploying OAI Object Pass\u001B[0m')
		else:
			logging.error('\u001B[1m Undeploying OAI Object Failed\u001B[0m')
		return success

	def CheckAndAddRoute(self, svrName, ipAddr, userName, password):
		logging.debug('Checking IP routing on ' + svrName)
		mySSH = SSH.SSHConnection()
		if svrName == 'porcepix':
			mySSH.open(ipAddr, userName, password)
			# Check if route to asterix gnb exists
			mySSH.command('ip route | grep --colour=never "192.168.68.64/26"', '\$', 10)
			result = re.search('172.21.16.127', mySSH.getBefore())
			if result is None:
				mySSH.command('echo ' + password + ' | sudo -S ip route add 192.168.68.64/26 via 172.21.16.127 dev eno1', '\$', 10)
			# Check if route to obelix enb exists
			mySSH.command('ip route | grep --colour=never "192.168.68.128/26"', '\$', 10)
			result = re.search('172.21.16.128', mySSH.getBefore())
			if result is None:
				mySSH.command('echo ' + password + ' | sudo -S ip route add 192.168.68.128/26 via 172.21.16.128 dev eno1', '\$', 10)
			# Check if forwarding is enabled
			mySSH.command('sysctl net.ipv4.conf.all.forwarding', '\$', 10)
			result = re.search('net.ipv4.conf.all.forwarding = 1', mySSH.getBefore())
			if result is None:
				mySSH.command('echo ' + password + ' | sudo -S sysctl net.ipv4.conf.all.forwarding=1', '\$', 10)
			# Check if iptables forwarding is accepted
			mySSH.command('echo ' + password + ' | sudo -S iptables -L FORWARD', '\$', 10)
			result = re.search('Chain FORWARD .*policy ACCEPT', mySSH.getBefore())
			if result is None:
				mySSH.command('echo ' + password + ' | sudo -S iptables -P FORWARD ACCEPT', '\$', 10)
			mySSH.close()
		if svrName == 'asterix':
			mySSH.open(ipAddr, userName, password)
			# Check if route to porcepix epc exists
			mySSH.command('ip route | grep --colour=never "192.168.61.192/26"', '\$', 10)
			result = re.search('172.21.16.136', mySSH.getBefore())
			if result is None:
				mySSH.command('echo ' + password + ' | sudo -S ip route add 192.168.61.192/26 via 172.21.16.136 dev em1', '\$', 10)
			# Check if route to porcepix cn5g exists
			mySSH.command('ip route | grep --colour=never "192.168.70.128/26"', '\$', 10)
			result = re.search('172.21.16.136', mySSH.getBefore())
			if result is None:
				mySSH.command('echo ' + password + ' | sudo -S ip route add 192.168.70.128/26 via 172.21.16.136 dev em1', '\$', 10)
			# Check if X2 route to obelix enb exists
			mySSH.command('ip route | grep --colour=never "192.168.68.128/26"', '\$', 10)
			result = re.search('172.21.16.128', mySSH.getBefore())
			if result is None:
				mySSH.command('echo ' + password + ' | sudo -S ip route add 192.168.68.128/26 via 172.21.16.128 dev em1', '\$', 10)
			# Check if forwarding is enabled
			mySSH.command('sysctl net.ipv4.conf.all.forwarding', '\$', 10)
			result = re.search('net.ipv4.conf.all.forwarding = 1', mySSH.getBefore())
			if result is None:
				mySSH.command('echo ' + password + ' | sudo -S sysctl net.ipv4.conf.all.forwarding=1', '\$', 10)
			# Check if iptables forwarding is accepted
			mySSH.command('echo ' + password + ' | sudo -S iptables -L FORWARD', '\$', 10)
			result = re.search('Chain FORWARD .*policy ACCEPT', mySSH.getBefore())
			if result is None:
				mySSH.command('echo ' + password + ' | sudo -S iptables -P FORWARD ACCEPT', '\$', 10)
			mySSH.close()
		if svrName == 'obelix':
			mySSH.open(ipAddr, userName, password)
			# Check if route to porcepix epc exists
			mySSH.command('ip route | grep --colour=never "192.168.61.192/26"', '\$', 10)
			result = re.search('172.21.16.136', mySSH.getBefore())
			if result is None:
				mySSH.command('echo ' + password + ' | sudo -S ip route add 192.168.61.192/26 via 172.21.16.136 dev eno1', '\$', 10)
			# Check if X2 route to asterix gnb exists
			mySSH.command('ip route | grep --colour=never "192.168.68.64/26"', '\$', 10)
			result = re.search('172.21.16.127', mySSH.getBefore())
			if result is None:
				mySSH.command('echo ' + password + ' | sudo -S ip route add 192.168.68.64/26 via 172.21.16.127 dev eno1', '\$', 10)
			# Check if X2 route to nepes gnb exists
			mySSH.command('ip route | grep --colour=never "192.168.68.192/26"', '\$', 10)
			result = re.search('172.21.16.137', mySSH.getBefore())
			if result is None:
				mySSH.command('echo ' + password + ' | sudo -S ip route add 192.168.68.192/26 via 172.21.16.137 dev eno1', '\$', 10)
			# Check if forwarding is enabled
			mySSH.command('sysctl net.ipv4.conf.all.forwarding', '\$', 10)
			result = re.search('net.ipv4.conf.all.forwarding = 1', mySSH.getBefore())
			if result is None:
				mySSH.command('echo ' + password + ' | sudo -S sysctl net.ipv4.conf.all.forwarding=1', '\$', 10)
			# Check if iptables forwarding is accepted
			mySSH.command('echo ' + password + ' | sudo -S iptables -L FORWARD', '\$', 10)
			result = re.search('Chain FORWARD .*policy ACCEPT', mySSH.getBefore())
			if result is None:
				mySSH.command('echo ' + password + ' | sudo -S iptables -P FORWARD ACCEPT', '\$', 10)
			mySSH.close()
		if svrName == 'nepes':
			mySSH.open(ipAddr, userName, password)
			# Check if route to ofqot gnb exists
			mySSH.command('ip route | grep --colour=never "192.168.68.192/26"', '\$', 10)
			result = re.search('172.21.16.109', mySSH.getBefore())
			if result is None:
				mySSH.command('echo ' + password + ' | sudo -S ip route add 192.168.68.192/26 via 172.21.16.109 dev enp0s31f6', '\$', 10)
			mySSH.command('sysctl net.ipv4.conf.all.forwarding', '\$', 10)
			result = re.search('net.ipv4.conf.all.forwarding = 1', mySSH.getBefore())
			if result is None:
				mySSH.command('echo ' + password + ' | sudo -S sysctl net.ipv4.conf.all.forwarding=1', '\$', 10)
			# Check if iptables forwarding is accepted
			mySSH.command('echo ' + password + ' | sudo -S iptables -L FORWARD', '\$', 10)
			result = re.search('Chain FORWARD .*policy ACCEPT', mySSH.getBefore())
			if result is None:
				mySSH.command('echo ' + password + ' | sudo -S iptables -P FORWARD ACCEPT', '\$', 10)
			mySSH.close()
		if svrName == 'ofqot':
			mySSH.open(ipAddr, userName, password)
			# Check if X2 route to nepes enb/epc exists
			mySSH.command('ip route | grep --colour=never "192.168.68.128/26"', '\$', 10)
			result = re.search('172.21.16.137', mySSH.getBefore())
			if result is None:
				mySSH.command('echo ' + password + ' | sudo -S ip route add 192.168.68.128/26 via 172.21.16.137 dev enp2s0', '\$', 10)
			# Check if forwarding is enabled
			mySSH.command('sysctl net.ipv4.conf.all.forwarding', '\$', 10)
			result = re.search('net.ipv4.conf.all.forwarding = 1', mySSH.getBefore())
			if result is None:
				mySSH.command('echo ' + password + ' | sudo -S sysctl net.ipv4.conf.all.forwarding=1', '\$', 10)
			# Check if iptables forwarding is accepted
			mySSH.command('echo ' + password + ' | sudo -S iptables -L FORWARD', '\$', 10)
			result = re.search('Chain FORWARD .*policy ACCEPT', mySSH.getBefore())
			if result is None:
				mySSH.command('echo ' + password + ' | sudo -S iptables -P FORWARD ACCEPT', '\$', 10)
			mySSH.close()

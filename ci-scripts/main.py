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
# Import Components
#-----------------------------------------------------------

import helpreadme as HELP
import constants as CONST


import cls_oaicitest		 #main class for OAI CI test framework
import cls_physim		 #class PhySim for physical simulators build and test
import cls_containerize	 #class Containerize for all container-based operations on RAN/UE objects
import cls_static_code_analysis  #class for static code analysis
import cls_physim1		 #class PhySim for physical simulators deploy and run
import cls_cluster		 # class for building/deploying on cluster
import cls_native        # class for all native/source-based operations

import sshconnection 
import epc
import ran
import cls_oai_html


#-----------------------------------------------------------
# Import Libs
#-----------------------------------------------------------
import sys		# arg
import re		# reg
import pexpect	# pexpect
import time		# sleep
import os
import subprocess
import xml.etree.ElementTree as ET
import logging
import datetime
import signal
import subprocess
import traceback
from multiprocessing import Process, Lock, SimpleQueue
logging.basicConfig(
	level=logging.DEBUG,
	stream=sys.stdout,
	format="[%(asctime)s] %(levelname)8s: %(message)s"
)




#-----------------------------------------------------------
# General Functions
#-----------------------------------------------------------



def CheckClassValidity(xml_class_list,action,id):
	if action not in xml_class_list:
		logging.error('test-case ' + id + ' has unlisted class ' + action + ' ##CHECK xml_class_list.yml')
		resp=False
	else:
		resp=True
	return resp


#assigning parameters to object instance attributes (even if the attributes do not exist !!)
def AssignParams(params_dict):

	for key,value in params_dict.items():
		setattr(CiTestObj, key, value)
		setattr(RAN, key, value)
		setattr(HTML, key, value)
		setattr(ldpc, key, value)



def ExecuteActionWithParam(action):
	global SSH
	global EPC
	global RAN
	global HTML
	global CONTAINERS
	global SCA
	global PHYSIM
	global CLUSTER
	global ldpc
	if action == 'Build_eNB' or action == 'Build_Image' or action == 'Build_Proxy' or action == "Build_Cluster_Image" or action == "Build_Run_Tests":
		RAN.Build_eNB_args=test.findtext('Build_eNB_args')
		CONTAINERS.imageKind=test.findtext('kind')
		forced_workspace_cleanup = test.findtext('forced_workspace_cleanup')
		RAN.Build_eNB_forced_workspace_cleanup=False
		CONTAINERS.forcedWorkspaceCleanup=False
		CLUSTER.forcedWorkspaceCleanup = False
		if forced_workspace_cleanup is not None and re.match('true', forced_workspace_cleanup, re.IGNORECASE):
			RAN.Build_eNB_forced_workspace_cleanup = True
			CONTAINERS.forcedWorkspaceCleanup = True
			CLUSTER.forcedWorkspaceCleanup = True
		eNB_instance=test.findtext('eNB_instance')
		if (eNB_instance is None):
			RAN.eNB_instance=0
			CONTAINERS.eNB_instance=0
		else:
			RAN.eNB_instance=int(eNB_instance)
			CONTAINERS.eNB_instance=int(eNB_instance)
		eNB_serverId=test.findtext('eNB_serverId')
		if (eNB_serverId is None):
			RAN.eNB_serverId[RAN.eNB_instance]='0'
			CONTAINERS.eNB_serverId[RAN.eNB_instance]='0'
		else:
			RAN.eNB_serverId[RAN.eNB_instance]=eNB_serverId
			CONTAINERS.eNB_serverId[CONTAINERS.eNB_instance]=eNB_serverId
		xmlBgBuildField = test.findtext('backgroundBuild')
		if (xmlBgBuildField is None):
			RAN.backgroundBuild=False
		else:
			if re.match('true', xmlBgBuildField, re.IGNORECASE):
				RAN.backgroundBuild=True
			else:
				RAN.backgroundBuild=False
		proxy_commit = test.findtext('proxy_commit')
		if proxy_commit is not None:
			CONTAINERS.proxyCommit = proxy_commit
		if action == 'Build_eNB':
			success = cls_native.Native.Build(HTML.testCase_id, HTML, RAN.eNBIPAddress, RAN.eNBSourceCodePath, RAN.Build_eNB_args)
		elif action == 'Build_Image':
			success = CONTAINERS.BuildImage(HTML)
		elif action == 'Build_Proxy':
			success = CONTAINERS.BuildProxy(HTML)
		elif action == 'Build_Cluster_Image':
			success = CLUSTER.BuildClusterImage(HTML)
		elif action == 'Build_Run_Tests':
			success = CONTAINERS.BuildRunTests(HTML)

	elif action == 'Initialize_eNB':
		RAN.eNB_Trace=test.findtext('eNB_Trace')
		RAN.eNB_Stats=test.findtext('eNB_Stats')
		datalog_rt_stats_file=test.findtext('rt_stats_cfg')
		if datalog_rt_stats_file is None:
			RAN.datalog_rt_stats_file='datalog_rt_stats.default.yaml'
		else:
			RAN.datalog_rt_stats_file=datalog_rt_stats_file
		RAN.Initialize_eNB_args=test.findtext('Initialize_eNB_args')
		eNB_instance=test.findtext('eNB_instance')
		USRPIPAddress=test.findtext('USRP_IPAddress')
		if USRPIPAddress is None:
			RAN.USRPIPAddress=''
		else:
			RAN.USRPIPAddress=USRPIPAddress
		if (eNB_instance is None):
			RAN.eNB_instance=0
		else:
			RAN.eNB_instance=int(eNB_instance)
		eNB_serverId=test.findtext('eNB_serverId')
		if (eNB_serverId is None):
			RAN.eNB_serverId[RAN.eNB_instance]='0'
		else:
			RAN.eNB_serverId[RAN.eNB_instance]=eNB_serverId
			
		#local variable air_interface
		air_interface = test.findtext('air_interface')		
		if (air_interface is None) or (air_interface.lower() not in ['nr','lte']):
			RAN.air_interface[RAN.eNB_instance] = 'lte-softmodem'
		else:
			RAN.air_interface[RAN.eNB_instance] = air_interface.lower() +'-softmodem'

		cmd_prefix = test.findtext('cmd_prefix')
		if cmd_prefix is not None: RAN.cmd_prefix = cmd_prefix
		success = RAN.InitializeeNB(HTML, EPC)

	elif action == 'Terminate_eNB':
		eNB_instance=test.findtext('eNB_instance')
		if (eNB_instance is None):
			RAN.eNB_instance=0
		else:
			RAN.eNB_instance=int(eNB_instance)
		eNB_serverId=test.findtext('eNB_serverId')
		if (eNB_serverId is None):
			RAN.eNB_serverId[RAN.eNB_instance]='0'
		else:
			RAN.eNB_serverId[RAN.eNB_instance]=eNB_serverId

		#retx checkers
		string_field=test.findtext('d_retx_th')
		if (string_field is not None):
			RAN.ran_checkers['d_retx_th'] = [float(x) for x in string_field.split(',')]
		string_field=test.findtext('u_retx_th')
		if (string_field is not None):
			RAN.ran_checkers['u_retx_th'] = [float(x) for x in string_field.split(',')]

		#local variable air_interface
		air_interface = test.findtext('air_interface')		
		if (air_interface is None) or (air_interface.lower() not in ['nr','lte']):
			RAN.air_interface[RAN.eNB_instance] = 'lte-softmodem'
		else:
			RAN.air_interface[RAN.eNB_instance] = air_interface.lower() +'-softmodem'
		success = RAN.TerminateeNB(HTML, EPC)

	elif action == 'Initialize_UE' or action == 'Attach_UE' or action == 'Detach_UE' or action == 'Terminate_UE' or action == 'CheckStatusUE' or action == 'DataEnable_UE' or action == 'DataDisable_UE':
		CiTestObj.ue_ids = test.findtext('id').split(' ')
		if test.findtext('nodes'):
			CiTestObj.nodes = test.findtext('nodes').split(' ')
			if len(CiTestObj.ue_ids) != len(CiTestObj.nodes):
				logging.error('Number of Nodes are not equal to the total number of UEs')
				sys.exit("Mismatch in number of Nodes and UIs")
		else:
			CiTestObj.nodes = [None] * len(CiTestObj.ue_ids)
		if action == 'Initialize_UE':
			success = CiTestObj.InitializeUE(HTML)
		elif action == 'Attach_UE':
			success = CiTestObj.AttachUE(HTML)
		elif action == 'Detach_UE':
			success = CiTestObj.DetachUE(HTML)
		elif action == 'Terminate_UE':
			success = CiTestObj.TerminateUE(HTML)
		elif action == 'CheckStatusUE':
			success = CiTestObj.CheckStatusUE(HTML)
		elif action == 'DataEnable_UE':
			success = CiTestObj.DataEnableUE(HTML)
		elif action == 'DataDisable_UE':
			success = CiTestObj.DataDisableUE(HTML)

	elif action == 'Ping':
		CiTestObj.ping_args = test.findtext('ping_args')
		CiTestObj.ping_packetloss_threshold = test.findtext('ping_packetloss_threshold')
		CiTestObj.ue_ids = test.findtext('id').split(' ')
		if test.findtext('nodes'):
			CiTestObj.nodes = test.findtext('nodes').split(' ')
			if len(CiTestObj.ue_ids) != len(CiTestObj.nodes):
				logging.error('Number of Nodes are not equal to the total number of UEs')
				sys.exit("Mismatch in number of Nodes and UIs")
		else:
			CiTestObj.nodes = [None] * len(CiTestObj.ue_ids)
		ping_rttavg_threshold = test.findtext('ping_rttavg_threshold') or ''
		success = CiTestObj.Ping(HTML,EPC,CONTAINERS)

	elif action == 'Iperf' or action == 'Iperf2_Unidir':
		CiTestObj.iperf_args = test.findtext('iperf_args')
		CiTestObj.ue_ids = test.findtext('id').split(' ')
		CiTestObj.svr_id = test.findtext('svr_id') or None
		if test.findtext('nodes'):
			CiTestObj.nodes = test.findtext('nodes').split(' ')
			if len(CiTestObj.ue_ids) != len(CiTestObj.nodes):
				logging.error('Number of Nodes are not equal to the total number of UEs')
				sys.exit("Mismatch in number of Nodes and UIs")
		else:
			CiTestObj.nodes = [None] * len(CiTestObj.ue_ids)
		if test.findtext('svr_node'):
			CiTestObj.svr_node = test.findtext('svr_node')
		CiTestObj.iperf_packetloss_threshold = test.findtext('iperf_packetloss_threshold')
		CiTestObj.iperf_bitrate_threshold = test.findtext('iperf_bitrate_threshold') or '90'
		CiTestObj.iperf_profile = test.findtext('iperf_profile') or 'balanced'
		CiTestObj.iperf_tcp_rate_target = test.findtext('iperf_tcp_rate_target') or None
		if CiTestObj.iperf_profile != 'balanced' and CiTestObj.iperf_profile != 'unbalanced' and CiTestObj.iperf_profile != 'single-ue':
			logging.error(f'test-case has wrong profile {CiTestObj.iperf_profile}, forcing balanced')
			CiTestObj.iperf_profile = 'balanced'
		CiTestObj.iperf_options = test.findtext('iperf_options') or 'check'
		if CiTestObj.iperf_options != 'check' and CiTestObj.iperf_options != 'sink':
			logging.error('test-case has wrong option ' + CiTestObj.iperf_options)
			CiTestObj.iperf_options = 'check'
		if action == 'Iperf':
			success = CiTestObj.Iperf(HTML, EPC, CONTAINERS)
		elif action == 'Iperf2_Unidir':
			success = CiTestObj.Iperf2_Unidir(HTML, EPC, CONTAINERS)

	elif action == 'IdleSleep':
		st = test.findtext('idle_sleep_time_in_sec') or "5"
		success = cls_oaicitest.IdleSleep(HTML, int(st))

	elif action == 'Build_PhySim':
		ldpc.buildargs  = test.findtext('physim_build_args')
		forced_workspace_cleanup = test.findtext('forced_workspace_cleanup')
		if (forced_workspace_cleanup is None):
			ldpc.forced_workspace_cleanup=False
		else:
			if re.match('true', forced_workspace_cleanup, re.IGNORECASE):
				ldpc.forced_workspace_cleanup=True
			else:
				ldpc.forced_workspace_cleanup=False
		success = ldpc.Build_PhySim(HTML,CONST)

	elif action == 'Deploy_Run_PhySim':
		success = PHYSIM.Deploy_PhySim(HTML)

	elif action == 'Initialize_MME':
		string_field = test.findtext('option')
		if (string_field is not None):
			EPC.mmeConfFile = string_field
		success = EPC.InitializeMME(HTML)

	elif action == 'Initialize_HSS' or action == 'Initialize_SPGW':
		if action == 'Initialize_HSS':
			success = EPC.InitializeHSS(HTML)
		elif action == 'Initialize_SPGW':
			success = EPC.InitializeSPGW(HTML)
	elif action == 'Terminate_HSS' or action == 'Terminate_MME' or action == 'Terminate_SPGW':
		if action == 'Terminate_HSS':
			success = EPC.TerminateHSS(HTML)
		elif action == 'Terminate_MME':
			success = EPC.TerminateMME(HTML)
		elif action == 'Terminate_SPGW':
			success = EPC.TerminateSPGW(HTML)

	elif action == 'Deploy_EPC':
		string_field = test.findtext('parameters')
		if (string_field is not None):
			EPC.yamlPath = string_field
		success = EPC.DeployEpc(HTML)

	elif action == 'Undeploy_EPC':
		success = EPC.UndeployEpc(HTML)

	elif action == 'Initialize_5GCN':
		string_field = test.findtext('args')
		if (string_field is not None):
			EPC.cfgDeploy = string_field	
		EPC.cnID = test.findtext('cn_id')
		success = EPC.Initialize5GCN(HTML)

	elif action == 'Terminate_5GCN':
		string_field = test.findtext('args')
		if (string_field is not None):
			EPC.cfgUnDeploy = string_field	
		EPC.cnID = test.findtext('cn_id')
		success = EPC.Terminate5GCN(HTML)

	elif action == 'Deploy_Object' or action == 'Undeploy_Object' or action == "Create_Workspace":
		eNB_instance=test.findtext('eNB_instance')
		if (eNB_instance is None):
			CONTAINERS.eNB_instance=0
		else:
			CONTAINERS.eNB_instance=int(eNB_instance)
		eNB_serverId=test.findtext('eNB_serverId')
		if (eNB_serverId is None):
			CONTAINERS.eNB_serverId[CONTAINERS.eNB_instance]='0'
		else:
			CONTAINERS.eNB_serverId[CONTAINERS.eNB_instance]=eNB_serverId
		string_field = test.findtext('yaml_path')
		if (string_field is not None):
			CONTAINERS.yamlPath[CONTAINERS.eNB_instance] = string_field
		string_field=test.findtext('d_retx_th')
		if (string_field is not None):
			CONTAINERS.ran_checkers['d_retx_th'] = [float(x) for x in string_field.split(',')]
		string_field=test.findtext('u_retx_th')
		if (string_field is not None):
			CONTAINERS.ran_checkers['u_retx_th'] = [float(x) for x in string_field.split(',')]
		string_field = test.findtext('services')
		if string_field is not None:
			CONTAINERS.services[CONTAINERS.eNB_instance] = string_field
		CONTAINERS.deploymentTag = cls_containerize.CreateTag(CONTAINERS.ranCommitID, CONTAINERS.ranBranch, CONTAINERS.ranAllowMerge)
		if action == 'Deploy_Object':
			success = CONTAINERS.DeployObject(HTML)
		elif action == 'Undeploy_Object':
			success = CONTAINERS.UndeployObject(HTML, RAN)
		elif action == 'Create_Workspace':
			success = CONTAINERS.Create_Workspace(HTML)

	elif action == 'Run_CUDATest' or action == 'Run_NRulsimTest' or action == 'Run_T2Test':
		ldpc.runargs = test.findtext('physim_run_args')
		ldpc.runsim = test.findtext('physim_run')
		ldpc.timethrs = test.findtext('physim_time_threshold')
		if action == 'Run_CUDATest':
			success = ldpc.Run_CUDATest(HTML,CONST,id)
		elif action == 'Run_NRulsimTest':
			success = ldpc.Run_NRulsimTest(HTML,CONST,id)
		elif action == 'Run_T2Test':
			success = ldpc.Run_T2Test(HTML,CONST,id)

	elif action == 'LicenceAndFormattingCheck':
		success = SCA.LicenceAndFormattingCheck(HTML)

	elif action == 'Cppcheck_Analysis':
		success = SCA.CppCheckAnalysis(HTML)

	elif action == 'Push_Local_Registry':
		string_field = test.findtext('registry_svr_id')
		if (string_field is not None):
			CONTAINERS.registrySvrId = string_field
		success = CONTAINERS.Push_Image_to_Local_Registry(HTML)

	elif action == 'Pull_Local_Registry':
		string_field = test.findtext('test_svr_id')
		if (string_field is not None):
			CONTAINERS.testSvrId = string_field
		CONTAINERS.imageToPull.clear()
		string_field = test.findtext('images_to_pull')
		if (string_field is not None):
			CONTAINERS.imageToPull = string_field.split()
		success = CONTAINERS.Pull_Image_from_Local_Registry(HTML)

	elif action == 'Clean_Test_Server_Images':
		string_field = test.findtext('test_svr_id')
		if (string_field is not None):
			CONTAINERS.testSvrId = string_field
		success = CONTAINERS.Clean_Test_Server_Images(HTML)

	elif action == 'Custom_Command':
		node = test.findtext('node')
		command = test.findtext('command')
		command_fail = test.findtext('command_fail') in ['True', 'true', 'Yes', 'yes']
		success = cls_oaicitest.Custom_Command(HTML, node, command, command_fail)

	elif action == 'Custom_Script':
		node = test.findtext('node')
		script = test.findtext('script')
		command_fail = test.findtext('command_fail') in ['True', 'true', 'Yes', 'yes']
		success = cls_oaicitest.Custom_Script(HTML, node, script, command_fail)

	elif action == 'Pull_Cluster_Image':
		string_field = test.findtext('images_to_pull')
		if (string_field is not None):
			CLUSTER.imageToPull = string_field.split()
		string_field = test.findtext('test_svr_id')
		if (string_field is not None):
			CLUSTER.testSvrId = string_field
		success = CLUSTER.PullClusterImage(HTML, RAN)

	else:
		logging.warning(f"unknown action {action}, skip step")
		success = True # by default, we skip the step and print a warning

	return success

#check if given test is in list
#it is in list if one of the strings in 'list' is at the beginning of 'test'
def test_in_list(test, list):
	for check in list:
		check=check.replace('+','')
		if (test.startswith(check)):
			return True
	return False

def receive_signal(signum, frame):
	sys.exit(1)






#-----------------------------------------------------------
# MAIN PART
#-----------------------------------------------------------

#loading xml action list from yaml
import yaml
xml_class_list_file='xml_class_list.yml'
if (os.path.isfile(xml_class_list_file)):
	yaml_file=xml_class_list_file
elif (os.path.isfile('ci-scripts/'+xml_class_list_file)):
	yaml_file='ci-scripts/'+xml_class_list_file
else:
	logging.error("XML action list yaml file cannot be found")
	sys.exit("XML action list yaml file cannot be found")

with open(yaml_file,'r') as f:
    # The FullLoader parameter handles the conversion-$
    #from YAML scalar values to Python dictionary format$
    xml_class_list = yaml.load(f,Loader=yaml.FullLoader)

mode = ''

CiTestObj = cls_oaicitest.OaiCiTest()
 
SSH = sshconnection.SSHConnection()
EPC = epc.EPCManagement()
RAN = ran.RANManagement()
HTML = cls_oai_html.HTMLManagement()
CONTAINERS = cls_containerize.Containerize()
SCA = cls_static_code_analysis.StaticCodeAnalysis()
PHYSIM = cls_physim1.PhySim()
CLUSTER = cls_cluster.Cluster()

ldpc=cls_physim.PhySim()    #create an instance for LDPC test using GPU or CPU build


#-----------------------------------------------------------
# Parsing Command Line Arguments
#-----------------------------------------------------------

import args_parse
py_param_file_present, py_params, mode = args_parse.ArgsParse(sys.argv,CiTestObj,RAN,HTML,EPC,ldpc,CONTAINERS,HELP,SCA,PHYSIM,CLUSTER)



#-----------------------------------------------------------
# TEMPORARY params management (UNUSED)
#-----------------------------------------------------------
#temporary solution for testing:
if py_param_file_present == True:
	AssignParams(py_params)

#-----------------------------------------------------------
# mode amd XML class (action) analysis
#-----------------------------------------------------------
cwd = os.getcwd()

if re.match('^TerminateeNB$', mode, re.IGNORECASE):
	if RAN.eNBIPAddress == '' or RAN.eNBUserName == '' or RAN.eNBPassword == '':
		HELP.GenericHelp(CONST.Version)
		sys.exit('Insufficient Parameter')
	if RAN.eNBIPAddress == 'none':
		sys.exit(0)
	RAN.eNB_instance=0
	RAN.eNB_serverId[0]='0'
	RAN.eNBSourceCodePath='/tmp/'
	RAN.TerminateeNB(HTML, EPC)
elif re.match('^TerminateHSS$', mode, re.IGNORECASE):
	if EPC.IPAddress == '' or EPC.UserName == '' or EPC.Password == '' or EPC.Type == '' or EPC.SourceCodePath == '':
		HELP.GenericHelp(CONST.Version)
		sys.exit('Insufficient Parameter')
	EPC.TerminateHSS(HTML)
elif re.match('^TerminateMME$', mode, re.IGNORECASE):
	if EPC.IPAddress == '' or EPC.UserName == '' or EPC.Password == '' or EPC.Type == '' or EPC.SourceCodePath == '':
		HELP.GenericHelp(CONST.Version)
		sys.exit('Insufficient Parameter')
	EPC.TerminateMME(HTML)
elif re.match('^TerminateSPGW$', mode, re.IGNORECASE):
	if EPC.IPAddress == '' or EPC.UserName == '' or EPC.Password == '' or EPC.Type == '' or EPC.SourceCodePath== '':
		HELP.GenericHelp(CONST.Version)
		sys.exit('Insufficient Parameter')
	EPC.TerminateSPGW(HTML)
elif re.match('^LogCollectBuild$', mode, re.IGNORECASE):
	if (RAN.eNBIPAddress == '' or RAN.eNBUserName == '' or RAN.eNBPassword == '' or RAN.eNBSourceCodePath == '') and (CiTestObj.UEIPAddress == '' or CiTestObj.UEUserName == '' or CiTestObj.UEPassword == '' or CiTestObj.UESourceCodePath == ''):
		HELP.GenericHelp(CONST.Version)
		sys.exit('Insufficient Parameter')
	if RAN.eNBIPAddress == 'none':
		sys.exit(0)
	CiTestObj.LogCollectBuild(RAN)
elif re.match('^LogCollecteNB$', mode, re.IGNORECASE):
	if RAN.eNBIPAddress == '' or RAN.eNBUserName == '' or RAN.eNBPassword == '' or RAN.eNBSourceCodePath == '':
		HELP.GenericHelp(CONST.Version)
		sys.exit('Insufficient Parameter')
	if os.path.isdir('cmake_targets/log'):
		cmd = 'zip -r enb.log.' + RAN.BuildId + '.zip cmake_targets/log'
		logging.info(cmd)
		try:
			zipStatus = subprocess.check_output(cmd, shell=True, stderr=subprocess.STDOUT, universal_newlines=True, timeout=60)
		except subprocess.CalledProcessError as e:
			logging.error("Command '{}' returned non-zero exit status {}.".format(e.cmd, e.returncode))
			logging.error("Error output:\n{}".format(e.output))
		sys.exit(0)
	RAN.LogCollecteNB()
elif re.match('^LogCollectHSS$', mode, re.IGNORECASE):
	if EPC.IPAddress == '' or EPC.UserName == '' or EPC.Password == '' or EPC.Type == '' or EPC.SourceCodePath == '':
		HELP.GenericHelp(CONST.Version)
		sys.exit('Insufficient Parameter')
	EPC.LogCollectHSS()
elif re.match('^LogCollectMME$', mode, re.IGNORECASE):
	if EPC.IPAddress == '' or EPC.UserName == '' or EPC.Password == '' or EPC.Type == '' or EPC.SourceCodePath == '':
		HELP.GenericHelp(CONST.Version)
		sys.exit('Insufficient Parameter')
	EPC.LogCollectMME()
elif re.match('^LogCollectSPGW$', mode, re.IGNORECASE):
	if EPC.IPAddress == '' or EPC.UserName == '' or EPC.Password == '' or EPC.Type == '' or EPC.SourceCodePath == '':
		HELP.GenericHelp(CONST.Version)
		sys.exit('Insufficient Parameter')
	EPC.LogCollectSPGW()
elif re.match('^LogCollectPing$', mode, re.IGNORECASE):
	if EPC.IPAddress == '' or EPC.UserName == '' or EPC.Password == '' or EPC.SourceCodePath == '':
		HELP.GenericHelp(CONST.Version)
		sys.exit('Insufficient Parameter')
	CiTestObj.LogCollectPing(EPC)
elif re.match('^LogCollectIperf$', mode, re.IGNORECASE):
	if EPC.IPAddress == '' or EPC.UserName == '' or EPC.Password == '' or EPC.SourceCodePath == '':
		HELP.GenericHelp(CONST.Version)
		sys.exit('Insufficient Parameter')
	CiTestObj.LogCollectIperf(EPC)
elif re.match('^LogCollectOAIUE$', mode, re.IGNORECASE):
	if CiTestObj.UEIPAddress == '' or CiTestObj.UEUserName == '' or CiTestObj.UEPassword == '' or CiTestObj.UESourceCodePath == '':
		HELP.GenericHelp(CONST.Version)
		sys.exit('Insufficient Parameter')
	CiTestObj.LogCollectOAIUE()
elif re.match('^InitiateHtml$', mode, re.IGNORECASE):
	count = 0
	foundCount = 0
	while (count < HTML.nbTestXMLfiles):
		#xml_test_file = cwd + "/" + CiTestObj.testXMLfiles[count]
		xml_test_file = sys.path[0] + "/" + CiTestObj.testXMLfiles[count]
		if (os.path.isfile(xml_test_file)):
			try:
				xmlTree = ET.parse(xml_test_file)
			except Exception as e:
				print(f"Error: {e} while parsing file: {xml_test_file}.")
			xmlRoot = xmlTree.getroot()
			HTML.htmlTabRefs.append(xmlRoot.findtext('htmlTabRef',default='test-tab-' + str(count)))
			HTML.htmlTabNames.append(xmlRoot.findtext('htmlTabName',default='test-tab-' + str(count)))
			HTML.htmlTabIcons.append(xmlRoot.findtext('htmlTabIcon',default='info-sign'))
			foundCount += 1
		count += 1
	if foundCount != HTML.nbTestXMLfiles:
		HTML.nbTestXMLfiles=foundCount
	
	HTML.CreateHtmlHeader()
elif re.match('^FinalizeHtml$', mode, re.IGNORECASE):
	logging.info('\u001B[1m----------------------------------------\u001B[0m')
	logging.info('\u001B[1m  Creating HTML footer \u001B[0m')
	logging.info('\u001B[1m----------------------------------------\u001B[0m')

	HTML.CreateHtmlFooter(CiTestObj.finalStatus)
elif re.match('^TesteNB$', mode, re.IGNORECASE) or re.match('^TestUE$', mode, re.IGNORECASE):
	logging.info('\u001B[1m----------------------------------------\u001B[0m')
	logging.info('\u001B[1m  Starting Scenario: ' + CiTestObj.testXMLfiles[0] + '\u001B[0m')
	logging.info('\u001B[1m----------------------------------------\u001B[0m')
	if re.match('^TesteNB$', mode, re.IGNORECASE):
		if RAN.eNBIPAddress == '' or RAN.ranRepository == '' or RAN.ranBranch == '' or RAN.eNBUserName == '' or RAN.eNBPassword == '' or RAN.eNBSourceCodePath == '' or EPC.IPAddress == '' or EPC.UserName == '' or EPC.Password == '' or EPC.Type == '' or EPC.SourceCodePath == '':
			HELP.GenericHelp(CONST.Version)
			if EPC.IPAddress == '' or EPC.UserName == '' or EPC.Password == '' or EPC.SourceCodePath == '' or EPC.Type == '':
				HELP.EPCSrvHelp(EPC.IPAddress, EPC.UserName, EPC.Password, EPC.SourceCodePath, EPC.Type)
			if RAN.ranRepository == '':
				HELP.GitSrvHelp(RAN.ranRepository, RAN.ranBranch, RAN.ranCommitID, RAN.ranAllowMerge, RAN.ranTargetBranch)
			if RAN.eNBIPAddress == ''  or RAN.eNBUserName == '' or RAN.eNBPassword == '' or RAN.eNBSourceCodePath == '':
				HELP.eNBSrvHelp(RAN.eNBIPAddress, RAN.eNBUserName, RAN.eNBPassword, RAN.eNBSourceCodePath)
			sys.exit('Insufficient Parameter')
	else:
		if CiTestObj.UEIPAddress == '' or CiTestObj.ranRepository == '' or CiTestObj.ranBranch == '' or CiTestObj.UEUserName == '' or CiTestObj.UEPassword == '' or CiTestObj.UESourceCodePath == '':
			HELP.GenericHelp(CONST.Version)
			sys.exit('UE: Insufficient Parameter')

	#read test_case_list.xml file
	# if no parameters for XML file, use default value
	if (HTML.nbTestXMLfiles != 1):
		xml_test_file = cwd + "/test_case_list.xml"
	else:
		xml_test_file = cwd + "/" + CiTestObj.testXMLfiles[0]

	xmlTree = ET.parse(xml_test_file)
	xmlRoot = xmlTree.getroot()

	exclusion_tests=xmlRoot.findtext('TestCaseExclusionList',default='')
	requested_tests=xmlRoot.findtext('TestCaseRequestedList',default='')
	if (HTML.nbTestXMLfiles == 1):
		HTML.htmlTabRefs.append(xmlRoot.findtext('htmlTabRef',default='test-tab-0'))
		HTML.htmlTabNames.append(xmlRoot.findtext('htmlTabName',default='Test-0'))
	all_tests=xmlRoot.findall('testCase')

	exclusion_tests=exclusion_tests.split()
	requested_tests=requested_tests.split()

	#check that exclusion tests are well formatted
	#(6 digits or less than 6 digits followed by +)
	for test in exclusion_tests:
		if     (not re.match('^[0-9]{6}$', test) and
				not re.match('^[0-9]{1,5}\+$', test)):
			logging.error('exclusion test is invalidly formatted: ' + test)
			sys.exit(1)
		else:
			logging.info(test)

	#check that requested tests are well formatted
	#(6 digits or less than 6 digits followed by +)
	#be verbose
	for test in requested_tests:
		if     (re.match('^[0-9]{6}$', test) or
				re.match('^[0-9]{1,5}\+$', test)):
			logging.info('test group/case requested: ' + test)
		else:
			logging.error('requested test is invalidly formatted: ' + test)
			sys.exit(1)
	if (EPC.IPAddress != '') and (EPC.IPAddress != 'none'):
		EPC.SetMmeIPAddress()
		EPC.SetAmfIPAddress()

	#get the list of tests to be done
	todo_tests=[]
	for test in requested_tests:
		if    (test_in_list(test, exclusion_tests)):
			logging.info('test will be skipped: ' + test)
		else:
			#logging.info('test will be run: ' + test)
			todo_tests.append(test)

	signal.signal(signal.SIGUSR1, receive_signal)

	HTML.CreateHtmlTabHeader()

	# On CI bench w/ containers, we need to validate if IP routes are set
	if EPC.IPAddress == '172.21.16.136':
		CONTAINERS.CheckAndAddRoute('porcepix', EPC.IPAddress, EPC.UserName, EPC.Password)
	if EPC.IPAddress == '172.21.16.137':
		CONTAINERS.CheckAndAddRoute('nepes', EPC.IPAddress, EPC.UserName, EPC.Password)
	if CONTAINERS.eNBIPAddress == '172.21.16.127':
		CONTAINERS.CheckAndAddRoute('asterix', CONTAINERS.eNBIPAddress, CONTAINERS.eNBUserName, CONTAINERS.eNBPassword)
	if CONTAINERS.eNB1IPAddress == '172.21.16.127':
		CONTAINERS.CheckAndAddRoute('asterix', CONTAINERS.eNB1IPAddress, CONTAINERS.eNB1UserName, CONTAINERS.eNB1Password)
	if CONTAINERS.eNBIPAddress == '172.21.16.128':
		CONTAINERS.CheckAndAddRoute('obelix', CONTAINERS.eNBIPAddress, CONTAINERS.eNBUserName, CONTAINERS.eNBPassword)
	if CONTAINERS.eNB1IPAddress == '172.21.16.128':
		CONTAINERS.CheckAndAddRoute('obelix', CONTAINERS.eNB1IPAddress, CONTAINERS.eNB1UserName, CONTAINERS.eNB1Password)
	if CONTAINERS.eNBIPAddress == '172.21.16.109' or CONTAINERS.eNBIPAddress == 'ofqot':
		CONTAINERS.CheckAndAddRoute('ofqot', CONTAINERS.eNBIPAddress, CONTAINERS.eNBUserName, CONTAINERS.eNBPassword)
	if CONTAINERS.eNBIPAddress == '172.21.16.137':
		CONTAINERS.CheckAndAddRoute('nepes', CONTAINERS.eNBIPAddress, CONTAINERS.eNBUserName, CONTAINERS.eNBPassword)
	if CONTAINERS.eNB1IPAddress == '172.21.16.137':
		CONTAINERS.CheckAndAddRoute('nepes', CONTAINERS.eNB1IPAddress, CONTAINERS.eNB1UserName, CONTAINERS.eNB1Password)

	task_set_succeeded = True
	HTML.startTime=int(round(time.time() * 1000))

	for test_case_id in todo_tests:
		for test in all_tests:
			id = test.get('id')
			if test_case_id != id:
				continue
			CiTestObj.testCase_id = id
			HTML.testCase_id=CiTestObj.testCase_id
			EPC.testCase_id=CiTestObj.testCase_id
			CiTestObj.desc = test.findtext('desc')
			always_exec = test.findtext('always_exec') in ['True', 'true', 'Yes', 'yes']
			HTML.desc=CiTestObj.desc
			action = test.findtext('class')
			if (CheckClassValidity(xml_class_list, action, id) == False):
				continue
			CiTestObj.ShowTestID()
			if not task_set_succeeded and not always_exec:
				msg = f"skipping test due to prior error"
				logging.warning(msg)
				HTML.CreateHtmlTestRowQueue(msg, "SKIP", [])
				break
			try:
				test_succeeded = ExecuteActionWithParam(action)
				if not test_succeeded:
					logging.error(f"test ID {test_case_id} action {action} failed ({test_succeeded}), skipping next tests")
					task_set_succeeded = False
			except Exception as e:
				s = traceback.format_exc()
				logging.error(f'while running CI, an exception occurred:\n{s}')
				HTML.CreateHtmlTestRowQueue("N/A", 'KO', [f"CI test code encountered an exception:\n{s}"])
				task_set_succeeded = False
				break

	if not task_set_succeeded:
		logging.error('\u001B[1;37;41mScenario failed\u001B[0m')
		HTML.CreateHtmlTabFooter(False)
		sys.exit('Failed Scenario')
	else:
		logging.info('\u001B[1;37;42mScenario passed\u001B[0m')
		HTML.CreateHtmlTabFooter(True)
elif re.match('^LoadParams$', mode, re.IGNORECASE):
	pass
else:
	HELP.GenericHelp(CONST.Version)
	sys.exit('Invalid mode')
sys.exit(0)

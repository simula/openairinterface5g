import sys
import logging
logging.basicConfig(
	level=logging.DEBUG,
	stream=sys.stdout,
	format="[%(asctime)s] %(levelname)8s: %(message)s"
)
import os
os.system(f'rm -rf cmake_targets')
os.system(f'mkdir -p cmake_targets/log')
import unittest

sys.path.append('./') # to find OAI imports below
import cls_oai_html
import cls_oaicitest
import cls_containerize
import ran

class TestDeploymentMethods(unittest.TestCase):
	def setUp(self):
		self.html = cls_oai_html.HTMLManagement()
		self.html.testCaseId = "000000"
		self.ci = cls_oaicitest.OaiCiTest()
		self.cont = cls_containerize.Containerize()
		self.ran = ran.RANManagement()
		self.cont.yamlPath[0] = ''
		self.cont.ranAllowMerge = True
		self.cont.ranBranch = ''
		self.cont.ranCommitID = ''
		self.cont.eNB_serverId[0] = '0'
		self.cont.eNBIPAddress = 'localhost'
		self.cont.eNBUserName = None
		self.cont.eNBPassword = None
		self.cont.eNBSourceCodePath = os.getcwd()

	def test_deploy(self):
		self.cont.yamlPath[0] = 'tests/simple-dep/'
		self.cont.deploymentTag = "noble"
		deploy = self.cont.DeployObject(self.html)
		undeploy = self.cont.UndeployObject(self.html, self.ran)
		self.assertTrue(deploy)
		self.assertTrue(undeploy)

	def test_deployfails(self):
		# fails reliably
		old = self.cont.yamlPath
		self.cont.yamlPath[0] = 'tests/simple-fail/'
		deploy = self.cont.DeployObject(self.html)
		self.cont.UndeployObject(self.html, self.ran)
		self.assertFalse(deploy)
		self.cont.yamlPath = old

	def test_deploy_ran(self):
		self.cont.yamlPath[0] = 'yaml_files/5g_rfsimulator_tdd_dora'
		self.cont.services[0] = "oai-gnb"
		self.cont.deploymentTag = 'develop-12345678'
		deploy = self.cont.DeployObject(self.html)
		undeploy = self.cont.UndeployObject(self.html, self.ran)
		self.assertTrue(deploy)
		self.assertTrue(undeploy)

	def test_deploy_multiran(self):
		self.cont.yamlPath[0] = 'yaml_files/5g_rfsimulator_tdd_dora'
		self.cont.services[0] = "oai-gnb oai-nr-ue"
		self.cont.deploymentTag = 'develop-12345678'
		deploy = self.cont.DeployObject(self.html)
		undeploy = self.cont.UndeployObject(self.html, self.ran)
		self.assertTrue(deploy)
		self.assertTrue(undeploy)

	def test_deploy_staged(self):
		self.cont.yamlPath[0] = 'yaml_files/5g_rfsimulator_tdd_dora'
		self.cont.services[0] = "oai-gnb"
		self.cont.deploymentTag = 'develop-12345678'
		deploy1 = self.cont.DeployObject(self.html)
		self.cont.services[0] = "oai-nr-ue"
		deploy2 = self.cont.DeployObject(self.html)
		undeploy = self.cont.UndeployObject(self.html, self.ran)
		self.assertTrue(deploy1)
		self.assertTrue(deploy2)
		self.assertTrue(undeploy)

if __name__ == '__main__':
	unittest.main()

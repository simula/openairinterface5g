import sys
import logging
logging.basicConfig(
	level=logging.DEBUG,
	stream=sys.stdout,
	format="[%(asctime)s] %(levelname)8s: %(message)s"
)
import os
import tempfile
os.system(f'rm -rf cmake_targets')
os.system(f'mkdir -p cmake_targets/log')
import unittest

sys.path.append('./') # to find OAI imports below
import cls_oai_html
import cls_oaicitest
import cls_containerize
from cls_ci_helper import TestCaseCtx
import ran
import cls_cmd

class TestDeploymentMethods(unittest.TestCase):
	def _pull_image(self, cmd, image):
		ret = cmd.run(f"docker inspect oai-ci/{image}:develop-12345678")
		if ret.returncode == 0: # exists
			return
		ret = cmd.run(f"docker pull oaisoftwarealliance/{image}:develop")
		self.assertEqual(ret.returncode, 0)
		ret = cmd.run(f"docker tag oaisoftwarealliance/{image}:develop oai-ci/{image}:develop-12345678")
		self.assertEqual(ret.returncode, 0)
		ret = cmd.run(f"docker rmi oaisoftwarealliance/{image}:develop")
		self.assertEqual(ret.returncode, 0)

	def setUp(self):
		with cls_cmd.getConnection("localhost") as cmd:
			self._pull_image(cmd, "oai-gnb")
			self._pull_image(cmd, "oai-nr-ue")
		self.html = cls_oai_html.HTMLManagement()
		self.html.testCaseId = "000000"
		self.ci = cls_oaicitest.OaiCiTest()
		self.cont = cls_containerize.Containerize()
		self.ran = ran.RANManagement()
		self.cont.yamlPath = ''
		self.cont.ranAllowMerge = True
		self.cont.ranBranch = ''
		self.cont.ranCommitID = ''
		self.cont.eNBSourceCodePath = os.getcwd()
		self.cont.num_attempts = 3
		self.node = 'localhost'
		self.ctx = TestCaseCtx.Default(tempfile.mkdtemp())
	def tearDown(self):
		with cls_cmd.LocalCmd() as c:
			c.run(f'rm -rf {self.ctx.logPath}')

	def test_deploy(self):
		self.cont.yamlPath = 'tests/simple-dep/'
		self.cont.deploymentTag = "noble"
		deploy = self.cont.DeployObject(self.ctx, self.node, self.html)
		undeploy = self.cont.UndeployObject(self.ctx, self.node, self.html, self.ran)
		self.assertTrue(deploy)
		self.assertTrue(undeploy)

	def test_deployfails(self):
		# fails reliably
		old = self.cont.yamlPath
		self.cont.yamlPath = 'tests/simple-fail/'
		deploy = self.cont.DeployObject(self.ctx, self.node, self.html)
		self.cont.UndeployObject(self.ctx, self.node, self.html, self.ran)
		self.assertFalse(deploy)
		self.cont.yamlPath = old

	def test_deployfails_2svc(self):
		# fails reliably
		old = self.cont.yamlPath
		self.cont.yamlPath = 'tests/simple-fail-2svc/'
		deploy = self.cont.DeployObject(self.ctx, self.node, self.html)
		self.cont.UndeployObject(self.ctx, self.node, self.html, self.ran)
		self.assertFalse(deploy)
		self.cont.yamlPath = old

	def test_deploy_ran(self):
		self.cont.yamlPath = 'yaml_files/5g_rfsimulator_tdd_dora'
		self.cont.services = "oai-gnb"
		self.cont.deploymentTag = 'develop-12345678'
		deploy = self.cont.DeployObject(self.ctx, self.node, self.html)
		undeploy = self.cont.UndeployObject(self.ctx, self.node, self.html, self.ran)
		self.assertTrue(deploy)
		self.assertTrue(undeploy)

	def test_deploy_multiran(self):
		self.cont.yamlPath = 'yaml_files/5g_rfsimulator_tdd_dora'
		self.cont.services = "oai-gnb oai-nr-ue"
		self.cont.deploymentTag = 'develop-12345678'
		deploy = self.cont.DeployObject(self.ctx, self.node, self.html)
		undeploy = self.cont.UndeployObject(self.ctx, self.node, self.html, self.ran)
		self.assertTrue(deploy)
		self.assertTrue(undeploy)

	def test_deploy_staged(self):
		self.cont.yamlPath = 'yaml_files/5g_rfsimulator_tdd_dora'
		self.cont.services = "oai-gnb"
		self.cont.deploymentTag = 'develop-12345678'
		deploy1 = self.cont.DeployObject(self.ctx, self.node, self.html)
		self.cont.services = "oai-nr-ue"
		deploy2 = self.cont.DeployObject(self.ctx, self.node, self.html)
		undeploy = self.cont.UndeployObject(self.ctx, self.node, self.html, self.ran)
		self.assertTrue(deploy1)
		self.assertTrue(deploy2)
		self.assertTrue(undeploy)

	def test_create_workspace(self):
		self.cont.eNBSourceCodePath = tempfile.mkdtemp()
		self.cont.ranRepository = "https://gitlab.eurecom.fr/oai/openairinterface5g.git"
		self.cont.ranCommitID = "05f9c975eeecbca1bdff5940affad44465f1301f"
		self.cont.ranBranch = "develop"
		ws = self.cont.Create_Workspace(self.node, self.html)
		with cls_cmd.LocalCmd() as cmd:
			cmd.run(f"rm -rf {self.cont.eNBSourceCodePath}")
		self.assertTrue(ws)

if __name__ == '__main__':
	unittest.main()

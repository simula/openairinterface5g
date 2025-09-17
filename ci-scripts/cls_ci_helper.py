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

import os
from typing import NamedTuple
import logging

class TestCaseCtx(NamedTuple):
    count: int
    test_id: int
    logPath: str

    def Default(logPath):
        return TestCaseCtx(123, 112233, logPath)
    def baseFilename(self):
        # typically, the test ID is of form 001234 (6 digits)
        return f"{self.logPath}/{self.count}-{self.test_id:06d}"

def archiveArtifact(cmd, ctx, remote_path):
    base = os.path.basename(remote_path)
    local = f"{ctx.baseFilename()}-{base}"
    logging.info(f"Archive artifact '{local}'")
    success = cmd.copyin(remote_path, local)
    if success:
        cmd.run(f'rm {remote_path}', silent=True)
    return local if success else None

# /*
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
# ---------------------------------------------------------------------
# file common/utils/data_recording/lib/common_utils.py
# brief Data Recording common utilities
# author Abdo Gaber
# date 2025
# version 1.0
# company Emerson, NI Test and Measurement
# email:
# note
# warning

import os
import json


def real_to_complex(real_vector):
    # Ensure the length of the real vector is even
    if len(real_vector) % 2 != 0:
        raise ValueError("The length of the real vector must be even.")

    # Split the real vector into real and imaginary parts
    real_part = real_vector[::2]
    imag_part = real_vector[1::2]

    # Combine the real and imaginary parts to form a complex vector
    complex_vector = real_part + 1j * imag_part

    return complex_vector


def write_config_data_recording_app_json(config_meta_data):
    if config_meta_data["data_recording_config"]["global_info"][
        "save_config_data_recording_app_json"
    ]:
        try:
            json.dumps(config_meta_data)
        except (TypeError, ValueError) as e:
            print(f"data_recording_config_meta_json is not JSON serializable: {e}")

        # Specify the file name
        output_file = (
            config_meta_data["data_recording_config"]["data_storage_path"]
            + "config_data_recording_app.json"
        )
        # Ensure the directory exists
        os.makedirs(os.path.dirname(output_file), exist_ok=True)

        # Write the JSON data to the file
        with open(output_file, "w") as file:
            try:
                json.dump(config_meta_data, file, indent=4)
                print(f"JSON file created successfully at {output_file}")
            except Exception as e:
                print(f"Failed to create JSON file: {e}")

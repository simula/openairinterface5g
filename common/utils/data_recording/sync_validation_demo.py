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
# file common/utils/data_recording/sync_validation_demo.py
# brief simple application to validate sync between payload captured on gNB and UE
# author Abdo Gaber
# date 2025
# version 1.0
# company Emerson, NI Test and Measurement
# email:
# note
# warning

import numpy as np
import os
import glob
import matplotlib.pyplot as plt
from lib import sigmf_interface
from lib import config_interface
import sigmf


def calculate_ber(capture_subdir: str, scope: str):
    """
    Iteratively counts all bit errors from data and finally calculates the BER for a given Eb/N0.
    """

    print("Start to calculate BER for subdirectory", capture_subdir)

    # get all collection files in the specified directory
    file_list = glob.glob(os.path.join(capture_subdir, "*.sigmf-collection"))
    print("Found Collection files:", len(file_list))

    bit_errors_total = 0
    num_bits_total = 0

    for idx, file in enumerate(file_list):
        print("Processing file:", file)

        # read SigMF collection file
        collection_file = sigmf.sigmffile.fromfile(file)
        print(" ")
        print("Collection file:", file)
        # extract filenames
        stream_files = collection_file.get_stream_names()
        print("Stream files:", stream_files)
        # extract filename from list of streams
        tx_bits_filename = next(
            filename for filename in stream_files if "tx-payload-bits" in filename
        )

        rx_bits_ref_rx_real_ce_filename = next(
            filename for filename in stream_files if "rx-payload-bits" in filename
        )
        # extract data and deserialized data (not used here)
        tx_bits, _, _ = sigmf_interface.load_sigmf(
            tx_bits_filename, capture_subdir, scope="tx-payload-bits"
        )
        rx_bits, _, rx_annotations = (
            sigmf_interface.load_sigmf(
                rx_bits_ref_rx_real_ce_filename, capture_subdir, scope="rx-payload-bits"
            )
        )

        # calculate bit errors
        bit_diffs = np.logical_xor(tx_bits, rx_bits)
        bit_errors = np.count_nonzero(bit_diffs)
        # increment total bit/symbol errors
        print("Current bit errors:", bit_errors / len(tx_bits))
        bit_errors_total += bit_errors
        num_bits_total += len(tx_bits)
        # plot "tx_bits" and "rx_bits_ref_rx_real_ce" for debugging purposes
        # Calculate the differences
        differences = np.array(tx_bits) - np.array(rx_bits)

        # Plot TX and RX bits using a scatter plot
        fig = plt.figure(figsize=(15, 6))
        indices = np.arange(len(tx_bits))

        plt.scatter(indices, tx_bits, label="TX bits", color="blue", marker="o", s=80)
        plt.scatter(indices, rx_bits, label="RX bits", color="red", marker="x", s=80)

        # Highlight differences
        for i in range(len(differences)):
            if differences[i] != 0:
                plt.scatter(i, 1.1, color="red", marker="|", s=100)

        plt.xlabel("Bit Index")
        plt.ylabel("Bit Value")
        plt.title(
            "TX vs RX Bits, Record "
            + str(idx)
            + ", Number of bits mismatchs: "
            + str(bit_errors),
            fontsize=16,
        )
        plt.legend()
        plt.grid(True)
        plt.ylim(-0.1, 1.2)
        plt.xlim(200, 250)
        plt.show(block=False)
        # Sleep for a specified amount of time (e.g., 5 seconds)
        plt.pause(6)
        plt.close(plt.gcf())  # Close the figure
        # Keep the script running to display the plots
        # input("Press Enter to continue...")

    # calculate BER
    ber = bit_errors_total / num_bits_total
    print("Total bit errors = ", ber)


def calculate_stats(main_config_file: str):

    scope = "real-world"

    print(f"Starting to generate statistics with data scope: {scope}")

    # read main config
    main_config = config_interface.read_main_config_file_json(main_config_file)

    # get paths for recorded captures
    capture_path = main_config["data_recording_config"]["data_storage_path"]
    calculate_ber(capture_path, scope)


if __name__ == "__main__":

    main_config = "/home/user/workarea/oai_recorded_data/config_data_recording_app.json"

    # calculate BER stats
    calculate_stats(main_config)

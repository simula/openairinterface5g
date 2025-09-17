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
# file common/utils/data_recording/lib/sigmf_interface.py
# brief SigMF Interface to Write data and meta-data to files in SigMF format
# author Abdo Gaber
# date 2025
# version 1.0
# company Emerson, NI Test and Measurement
# email:
# note
# warning

import os
import sigmf
from sigmf import SigMFFile
# from sigmf.utils import get_data_type_str
import numpy as np
from datetime import datetime
import yaml
from sigmf import SigMFCollection

"""
SERIALIZATION_SCHEMES = {
    # gnb
    "rx-fd-data": ["subcarriers", "ofdm_symbols"],
    "tx-pilots-fd-data": ["subcarriers", "ofdm_symbols"],
    "raw-ce-fd-data": ["subcarriers", "ofdm_symbols"],
    "rx-payload-bits": ["bits", "subcarriers", "ofdm_symbols"],
    # ue
    "tx-scrambled-bits": ["bits", "subcarriers", "ofdm_symbols"],
    "tx-payload-bits": ["bits", "subcarriers", "ofdm_symbols"],
}
"""
STANDARDS = {"5gnr_oai": "5gnr"}


def time_stamp_formating(time_stamp, datetime_offset):
    # Parse the input string into a datetime object
    time_stamp_ms_obj = datetime.strptime(time_stamp, "%Y%m%d_%H%M%S%f")

    # Format the datetime object into the desired output format with milliseconds
    time_stamp_ms_iso = (
        time_stamp_ms_obj.strftime("%Y_%m_%dT%H:%M:%S.%f")[:-3] + datetime_offset
    )
    time_stamp_ms = time_stamp_ms_iso
    time_stamp_ms_file_name = time_stamp_ms_iso.replace(":", "_").replace(".", "_")

    return time_stamp_ms, time_stamp_ms_file_name


def create_serialization_metadata(serialization_scheme, data_source: str,
                                  link_sim_parameters: dict) -> dict:
    """Creates dict that specifies the serialization metadata."""

    # retrieve parameter from LinkSimulator config
    if data_source == "5gnr_oai":
        num_ofdm_symbol = link_sim_parameters["nr_of_symbols"]
        num_subcarriers = link_sim_parameters["rb_size"] * 12
        num_bits_per_symbol = link_sim_parameters["qam_mod_order"]
    else:
        raise Exception(
            f"Invalid data source string '{data_source}'! Only '5gnr_oai' is valid!"
        )

    # create array with scheme dimensions
    if len(serialization_scheme) == 2:
        counts = [num_subcarriers, num_ofdm_symbol]
    elif len(serialization_scheme) == 3:
        counts = [num_bits_per_symbol, num_subcarriers, num_ofdm_symbol]
    else:
        raise Exception(
            f"Invalid number of elements in serialization scheme: {len(serialization_scheme)}")

    # create metadata dict
    serialization_dict = {
        "serialization:scheme": serialization_scheme,
        "serialization:counts": counts,
        }

    return serialization_dict


def map_metadata_to_sigmf_format(scope, waveform_generator, parameter_map_file, captured_data):
    """
    Maps metadata from Waveform creator to API and SigMF format.
    The used parameters and the mapping pairs are specified in a separate YAML file
    that must be provided as well.
    """
    # read waveform parameter map from yaml file
    dir_path = os.path.dirname(__file__)
    src_path = os.path.split(dir_path)[0]

    with open(os.path.join(src_path, parameter_map_file), "r") as file:
        parameter_map_dic = yaml.load(file, Loader=yaml.Loader)
    # preallocate target dict
    sigmf_metadata_dict = {}

    #  get standard and name of generator
    standard_key = parameter_map_dic["waveform_generator"][waveform_generator]
    generator = standard_key["generator"]

    if scope == "tx":
        parameter_map_dic = parameter_map_dic["transmitter"][
            STANDARDS[waveform_generator]
        ]
    elif scope == "channel":
        parameter_map_dic = parameter_map_dic["channel"]
    elif scope == "rx":
        parameter_map_dic = parameter_map_dic["receiver"]
    else:
        raise Exception(
            f"Invalid mapping scope '{scope}'! Only 'tx', 'channel' and 'rx' are valid!"
        )

    # check if standard key is given
    if parameter_map_dic is None:
        raise Exception(
            "Invalid standard key: "
            "Name should be corrected or added to wireless_link_parameter_map.yaml, given: "
            f"{waveform_generator}"
        )

    for parameter_pair in parameter_map_dic:
        # check if key for chosen simulator even exists
        if waveform_generator + "_parameter" in parameter_pair.keys():
            # only continue with mapping from file if direct equivalent exists
            if parameter_pair[waveform_generator + "_parameter"]["name"]:
                # It is not necessary to get all parameters from wireless_link_parameter_map.yaml
                # in captured_data since some parameters related to DL or UL only
                if (parameter_pair[waveform_generator + "_parameter"]["name"] in captured_data.keys()):
                    # extract value from waveform config source
                    value = captured_data[
                        parameter_pair[waveform_generator + "_parameter"]["name"]]
                    # additional mapping if parameter values should come from a discrete set of values
                    if ("value_map" in parameter_pair[waveform_generator + "_parameter"].keys()):
                        value = parameter_pair[waveform_generator + "_parameter"]["value_map"][value]
                    # write to target dictionary for SigMF
                    sigmf_metadata_dict[parameter_pair["sigmf_parameter_name"]] = value
            else:
                raise Exception(
                    f"Incomplete specification in field '{waveform_generator}_parameter'!")
        # else:  # fill_non_explicit_fields
        #     waveform_config[parameter_pair["sigmf_parameter_name"]] = "none"
    if not sigmf_metadata_dict:
        raise Exception(
            """ERROR: Check captured meta-data or provided config and meta data """)

    # check for non-JSON-serializable data types
    def isfloat(NumberString):
        try:
            float(NumberString)
            return True
        except ValueError:
            return False

    for key, value in sigmf_metadata_dict.items():
        if isinstance(value, np.integer):
            sigmf_metadata_dict[key] = int(value)
        elif isinstance(value, (np.float16, np.float32, np.float64)):
            sigmf_metadata_dict[key] = np.format_float_positional(value, trim="-")
        elif isinstance(value, int):
            sigmf_metadata_dict[key] = int(value)
        elif isinstance(value, float):
            # store value in decimal and not in scientific notation
            sigmf_metadata_dict[key] = float(value)
        elif isinstance(value, str) and key != "standard":
            # convert string to lower case
            sigmf_metadata_dict[key] = value.lower()
            if isfloat(value):
                if value.isdigit():
                    sigmf_metadata_dict[key] = int(float(value))
                elif value.replace(".", "", 1).isdigit() and value.count(".") < 2:
                    sigmf_metadata_dict[key] = float(value)

    return sigmf_metadata_dict, generator


def create_system_components_metadata(waveform_generator, parameter_map_file, captured_data):
    """Creates system components (TX, channel, RX) metadata that will reside in the annotations."""
    # map metadata of waveform generator to SigMF format
    signal_info, generator = map_metadata_to_sigmf_format(
        "tx", waveform_generator, parameter_map_file, captured_data
    )
    tx_metadata = {
        "signal:detail": {
            "standard": STANDARDS[waveform_generator],
            "generator": generator,
            STANDARDS[waveform_generator]: signal_info,
        }
    }

    channel_metadata = {}   # will be filled later
    rx_metadata = {}        # will be filled later

    return tx_metadata, channel_metadata, rx_metadata


def write_recorded_data_to_sigmf(captured_data, config_meta_data, global_info, idx):
    """
    Compiles and saves provided data and metadata into SigMF file format.
    """
    # get meta data from config file
    base_station_meta_data = config_meta_data["data_recording_config"]["base_station"][
        "meta_data"]
    user_equipment_meta_data = config_meta_data["data_recording_config"][
        "user_equipment"]["meta_data"]

    # Check the receive target path is valid, else create folder
    data_storage_path = config_meta_data["data_recording_config"]["data_storage_path"]
    if not os.path.isdir(data_storage_path):
        print("Create new folder for recorded data: " + str(data_storage_path))
        os.makedirs(data_storage_path)

    # Write recorded data to file
    # Get time stamp
    time_stamp_ms, time_stamp_ms_file_name = time_stamp_formating(
        captured_data["time_stamp"], global_info["datetime_offset"])
    # Map OAI Message Name to SigMF Message Name
    file_name_prefix = config_meta_data["data_recording_config"][
        "supported_oai_tracer_messages"][captured_data["message_type"]]["file_name_prefix"]
    recorded_data_file_name = (
        file_name_prefix + "-rec-" + str(idx) + "-" + time_stamp_ms_file_name)

    dataset_filename = recorded_data_file_name + ".sigmf-data"
    dataset_file_path = os.path.join(data_storage_path, dataset_filename)
    print(dataset_file_path)
    captured_data["recorded_data"].tofile(dataset_file_path)

    # map OAI config data to SigMF metadata
    waveform_generator = config_meta_data["data_recording_config"]["global_info"][
        "waveform_generator"]
    parameter_map_file = config_meta_data["data_recording_config"]["parameter_map_file"]
    tx_metadata, channel_metadata, rx_metadata = create_system_components_metadata(
        waveform_generator, parameter_map_file, captured_data)

    # Add other OAI metadata to SigMF metadata
    # ----------------------------------------------------
    # Set 1: Parameters needs to be derived from OAI message
    # ----------------------------------------------------
    # PUSCH DMRS: OFDM symbol start index within slot
    get_pusch_dmrs_start_ofdm_symbol = 0
    tx_metadata["signal:detail"][STANDARDS[waveform_generator]][
        "pusch_dmrs:ofdm_symbol_idx"] = []

    for symbol in range(captured_data["start_symbol_index"],
                        captured_data["start_symbol_index"] + captured_data["nr_of_symbols"]):

        dmrs_symbol_flag = (captured_data["ul_dmrs_symb_pos"] >> symbol) & 0x01
        if dmrs_symbol_flag and not get_pusch_dmrs_start_ofdm_symbol:
            get_pusch_dmrs_start_ofdm_symbol = 1
            tx_metadata["signal:detail"][STANDARDS[waveform_generator]][
                "pusch_dmrs:start_ofdm_symbol"] = symbol
        if dmrs_symbol_flag:
            tx_metadata["signal:detail"][STANDARDS[waveform_generator]][
                "pusch_dmrs:ofdm_symbol_idx"].append(symbol)  # Append symbol to the list

    tx_metadata["signal:detail"][STANDARDS[waveform_generator]][
        "pusch_dmrs:duration_num_ofdm_symbols"] = 1
    tx_metadata["signal:detail"][STANDARDS[waveform_generator]][
        "pusch_dmrs:num_add_positions"] = (captured_data["number_dmrs_symbols"] - 1)

    # If the message is a BIT message, add number of bits to the signal info
    # "UE_PHY_UL_SCRAMBLED_TX_BITS", "GNB_PHY_UL_PAYLOAD_RX_BITS", "UE_PHY_UL_PAYLOAD_TX_BITS"
    if "_BITS" in captured_data["message_type"]:
        # if "BIT" in captured_data["message_type"]:
        tx_metadata["signal:detail"][STANDARDS[waveform_generator]]["num_bits"] = (
            captured_data["number_of_bits"])

    # If the message is captured from UE, it is UPLink message,
    # so number of antennas is num of Tx antennas
    # number of antennas on Rx side should be read from global_info given by user
    if "UE" in captured_data["message_type"]:
        tx_metadata["signal:detail"][STANDARDS[waveform_generator]][
            "num_tx_antennas"
        ] = captured_data["nb_antennas"]
        rx_metadata.update(
            {"num_rx_antennas": base_station_meta_data["num_rx_antennas"]}
        )
    # If the message is captured from gNB, it is UPLink message,
    # so number of antennas is num of Rx antennas
    # number of antennas on Tx side should be read from global_info given by user
    else:
        tx_metadata["signal:detail"][STANDARDS[waveform_generator]][
            "num_tx_antennas"] = user_equipment_meta_data["num_tx_antennas"]
        rx_metadata.update({"num_rx_antennas": captured_data["nb_antennas"]})

    # Check if the message type contains "UL"
    if "UL" in captured_data["message_type"]:
        tx_metadata["signal:detail"][STANDARDS[waveform_generator]][
            "link_direction"] = "uplink"
    else:
        tx_metadata["signal:detail"][STANDARDS[waveform_generator]][
            "link_direction"] = "downlink"
    # ----------------------------------------------------
    # Set 2: Parameters that is hardcoded
    # ----------------------------------------------------
    tx_metadata["signal:detail"][STANDARDS[waveform_generator]][
        "pusch:content"] = "compliant"
    tx_metadata["signal:detail"][STANDARDS[waveform_generator]][
        "pusch:mapping_type"] = "B"
    tx_metadata["signal:detail"][STANDARDS[waveform_generator]]["num_slots"] = 1
    if config_meta_data["test_config"]["test_mode"] == "rf_simulation":
        tx_metadata["signal:detail"][STANDARDS[waveform_generator]][
            "pusch:payload_bit_pattern"] = "random"
    elif config_meta_data["test_config"]["test_mode"] == "rf_real_time":
        tx_metadata["signal:detail"][STANDARDS[waveform_generator]][
            "pusch:payload_bit_pattern"] = "zeros"
    else:
        raise Exception("ERROR: Test mode is not supported in SigMF formatting")
    # ----------------------------------------------------
    # Read mean parameters
    freq = config_meta_data["environment_emulation"]["target_link_config"][
        "wireless_channel"]["carrierFreqValueList_hz"][0]
    bandwidth = config_meta_data["data_recording_config"]["common_meta_data"][
        "bandwidth"]
    # used_signal_bandwidth = (
    #    pow(10, 6) * config_meta_data["environment_emulation"]["target_link_config"]["ran_config"][
    #        "uplink"]["ul_used_signal_BW_MHz"])
    sample_rate = config_meta_data["data_recording_config"]["common_meta_data"][
        "sample_rate"]

    # get signal emitter info
    signal_emitter = {
        "manufacturer": "NI",
        "seid": user_equipment_meta_data["seid"],  # Unique ID of the emitter
        "hw": user_equipment_meta_data["hw_type"],
        "hw_subtype": user_equipment_meta_data["hw_subtype"],
        "frequency": freq,
        "sample_rate": sample_rate,
        "bandwidth": bandwidth,
        "gain_tx": user_equipment_meta_data["tx_gain"],
        "clock_reference": config_meta_data["data_recording_config"][
            "common_meta_data"
        ]["clock_reference"],
        }

    # ----------------------------------------------------
    # get channel metadata
    # ----------------------------------------------------
    # get downlink channel info
    # ch_downlink_config = config_meta_data["environment_emulation"]["target_link_config"]["wireless_channel"]["downlink"]
    # ch_downlink ={"channel_model": ch_downlink_config["type"]}

    # ch_downlink_model = {"channel_model": "AWGN"}
    # get uplink channel info
    ch_uplink_config = config_meta_data["environment_emulation"]["target_link_config"][
        "wireless_channel"
    ]["uplink"]
    # Real Time RF Emulation
    if config_meta_data["test_config"]["test_mode"] == "rf_real_time":
        channel_metadata.update({"emulation_mode": "ni rf real time"})
        if ch_uplink_config["type"] == "statistical_predefined":
            channel_metadata.update(
                {
                    "channel_model": ch_uplink_config["statistical"]["predef_channel_profile"],
                    "snr_esn0_db": ch_uplink_config["statistical"]["snr_db"],
                    }
            )

        elif ch_uplink_config["type"] == "statistical_var":
            channel_metadata.update(
                {
                    "channel_model": ch_uplink_config["statistical"]["var_power_delay_profile"],
                    "snr_esn0_db": ch_uplink_config["statistical"]["snr_db"],
                    "delay_spread": ch_uplink_config["statistical"]["delay_spread_ns"] * pow(10, -9),
                    "speed": ch_uplink_config["statistical"]["speed_mps"],
                    }
            )
        else:
            raise Exception("ERROR: channel type is not supported in SigMF formatting")
    # RF Simulation
    elif config_meta_data["test_config"]["test_mode"] == "rf_simulation":
        channel_metadata.update({"emulation_mode": "oai rf simulation"})
        channel_metadata.update(
            {
                "channel_model": ch_uplink_config["statistical"]["predef_channel_profile"],
                "snr_esn0_db": ch_uplink_config["statistical"]["snr_db"],
                "delay_spread": ch_uplink_config["statistical"]["delay_spread_ns"]
                * pow(10, -9), "speed": ch_uplink_config["statistical"]["speed_mps"],
            }
        )
    else:
        raise Exception(
            "ERROR: Channel Emulation mode is not supported in SigMF formatting")

    channel_metadata["carrier_frequency"] = config_meta_data["environment_emulation"][
        "target_link_config"]["wireless_channel"]["carrierFreqValueList_hz"][0]

    # ----------------------------------------------------
    # get rx metadata
    # ----------------------------------------------------
    rx_metadata.update(
        {
            "bandwidth": bandwidth,
            "gain": base_station_meta_data["rx_gain"],
            "manufacturer": "NI",
            "seid": base_station_meta_data["seid"],
            "hw_subtype": base_station_meta_data["hw_subtype"],
            "clock_reference": config_meta_data["data_recording_config"][
                "common_meta_data"
            ]["clock_reference"],
            "phy_freq_domain_receiver_type": config_meta_data["test_config"][
                "dut_type"
            ],
        }
    )
    # ----------------------------------------------------
    # get test config
    # ----------------------------------------------------
    """
    test_config = {
        "test_name": config_meta_data["test_config"]["test_name"],
        "test_mode": config_meta_data["test_config"]["test_mode"],
        "dut_type": config_meta_data["test_config"]["dut_type"],
        }
    """
    # Create sigmf metadata
    # ----------------------
    # Add global parameters to SigMF metadata
    # ----------------------
    meta = SigMFFile(
        data_file=dataset_file_path,  # extension is optional
        global_info={
            SigMFFile.DATATYPE_KEY: captured_data[
                "sigmf_data_type"
            ],  # "cf32_le",  # get_data_type_str(rx_data) - 'cf64_le' is not supported yet
            SigMFFile.SAMPLE_RATE_KEY: sample_rate,
            SigMFFile.NUM_CHANNELS_KEY: 1,
            SigMFFile.AUTHOR_KEY: global_info["author"],
            SigMFFile.DESCRIPTION_KEY: config_meta_data["data_recording_config"][
                "supported_oai_tracer_messages"
            ][captured_data["message_type"]]["description"],
            SigMFFile.RECORDER_KEY: "NI Data Recording Application for OAI",
            SigMFFile.LICENSE_KEY: "MIT License",
            # Since we are focusing on 5G NR UL, the base station is the receiver
            SigMFFile.HW_KEY: base_station_meta_data["hw_type"],
            # Disable DATASET key to mitigate the warning when read SIGMF data although it is given in the spec.
            # It seems SIGMF still has bug here
            SigMFFile.DATASET_KEY: dataset_filename,
            SigMFFile.VERSION_KEY: sigmf.__version__,
            sigmf.SigMFFile.COLLECTION_KEY: global_info["collection_file"],
        },
    )

    # ----------------------
    # Add capture parameters to SigMF metadata
    # ----------------------
    serialization_scheme = config_meta_data["data_recording_config"][
        "supported_oai_tracer_messages"
    ][captured_data["message_type"]]["serialization_scheme"]
    capture_metadata = {
        sigmf.SigMFFile.DATETIME_KEY: time_stamp_ms,
        SigMFFile.FREQUENCY_KEY: freq,
        **create_serialization_metadata(
            serialization_scheme, waveform_generator, captured_data
        ),
    }
    meta.add_capture(start_index=0, metadata=capture_metadata)  # Sample Start

    # create annotations dict
    system_components_dict = {
        "system_components:transmitter": [
            {
                "transmitter_id": "tx_0",
                **tx_metadata,
                "signal:emitter": signal_emitter,
            }
        ]
    }

    system_components_dict["system_components:channel"] = [
        {"transmitter_id": "tx_0", **channel_metadata}]

    system_components_dict["system_components:receiver"] = rx_metadata
    # ----------------------
    # Add annotation parameters to SigMF metadata
    # ----------------------
    meta.add_annotation(
        start_index=0,  # Sample Start
        length=len(captured_data["recorded_data"]),  # Sample count
        metadata={
            # SigMFFile.FLO_KEY: freq - sample_rate/ 2,  # args.freq - args.rate / 2,
            # SigMFFile.FHI_KEY: freq + sample_rate/ 2,  # args.freq + args.rate / 2,
            # SigMFFile.LABEL_KEY: label,
            # SigMFFile.COMMENT_KEY: general_config["comment"],
            "num_transmitters": 1,
            **system_components_dict,
        },
    )

    # Write Meta Data to file
    dataset_meta_filename = recorded_data_file_name + ".sigmf-meta"
    dataset_meta_file_path = os.path.join(data_storage_path, dataset_meta_filename)
    meta.tofile(dataset_meta_file_path)  # extension is optional

    print(dataset_meta_file_path)

    return dataset_meta_file_path


def save_sigmf_collection(streams: list, global_info: dict, description: str, storage_path: str):
    """Save metadata and links to SigMF data/metadata files to SigMF collection file."""
    # construct path
    storage_path = os.path.expanduser(storage_path)
    collection_filepath = os.path.join(storage_path, global_info["collection_file"])
    # statically configure Collection metadata
    collection = SigMFCollection(
        streams,
        metadata={
            SigMFCollection.COLLECTION_KEY: {
                SigMFCollection.VERSION_KEY: sigmf.__version__,
                SigMFCollection.DESCRIPTION_KEY: description,
                SigMFCollection.AUTHOR_KEY: global_info["author"],
                # SigMFCollection.LICENSE_KEY: "License",
                # SigMFCollection.EXTENSIONS_KEY: global_info["extensions"],
                SigMFCollection.STREAMS_KEY: [],
            }
        },
    )

    # save collection file
    collection.tofile(collection_filepath)
    print("")
    print(collection_filepath + ".sigmf-collection")


def load_sigmf(filename: str, storage_path: str, scope: str):
    """
    Loads metadata and data from SigMF file.
    """
    # construct path
    storage_path = os.path.expanduser(storage_path)
    filepath = os.path.join(storage_path, filename)
    # load SigMF file
    signal = sigmf.sigmffile.fromfile(filepath)

    # get metadata fields
    global_metadata = signal.get_global_info()
    annotations_metadata = signal.get_annotations()

    # we only consider single annotations metadata element per SigMF file
    annotation_start_idx = annotations_metadata[0][sigmf.SigMFFile.START_INDEX_KEY]
    annotation_length = annotations_metadata[0][sigmf.SigMFFile.LENGTH_INDEX_KEY]

    # get capture metadata
    capture_metadata = signal.get_capture_info(annotation_start_idx)

    # from source code: sigmffile.py
    # "autoscale : bool, default True
    #   If dataset is in a fixed-point representation, scale samples from (min, max) to (-1.0, 1.0)
    # raw_components : bool, default False
    #   If True read and return the sample components (individual I & Q for complex, samples for real)
    #   with no conversions or interleaved channels.""
    # TODO: problem: raw_components flag is not used at all! changes in source code required!
    # TODO: data is always converted to float32 after being read

    # read actual data
    data = signal.read_samples(
        annotation_start_idx, annotation_length, autoscale=False, raw_components=True)

    return data, global_metadata, annotations_metadata

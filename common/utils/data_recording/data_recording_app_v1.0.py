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
# file common/utils/data_recording/data_recording_app_v1.0.py
# brief main application of synchronized real-time data recording
# author Abdo Gaber
# date 2025
# version 1.0
# company Emerson, NI Test and Measurement
# email:
# note
# warning

import sysv_ipc as ipc
import struct
import time
from datetime import datetime
from termcolor import colored
import numpy as np
import json
import concurrent.futures
# from concurrent import futures
import threading
# import related functions
from lib import sigmf_interface
# import library functions
from lib import sync_service
from lib import data_recording_messages_def
from lib import common_utils
from lib import config_interface

DEBUG_WIRELESS_RECORDED_DATA = True
DEBUG_BUFFER_READING = False

# globally applicable metadata
global_info = {
    "author": "Abdo Gaber",
    "description": "Synchronized Real-Time Data Recording",
    "timestamp": 0,
    "collection_file_prefix": "data-collection",  # collection file name prefix "deap-rx + str(...)"
    "collection_file": "",  # Reserved to be created in the code: “data-collection_rec-0_TIME-STAMP”
    "datetime_offset": "",  # datetime offset between current location and UTC/Zulu timezone
    # Example: "+01:00" for Berlin, Germany
    "save_config_data_recording_app_json": True,
    "waveform_generator": "5gnr_oai",
    "extensions": {},
}

# Supported OAI Trace messages
# UL receiver messages
# gNB IQ Msgs: "GNB_PHY_UL_FD_PUSCH_IQ", "GNB_PHY_UL_FD_DMRS", "GNB_PHY_UL_FD_CHAN_EST_DMRS_POS",
#               "GNB_PHY_UL_FD_CHAN_EST_DMRS_INTERPL"
# gNB BITS Msgs: "GNB_PHY_UL_PAYLOAD_RX_BITS"
# UE BITS Msgs: "UE_PHY_UL_SCRAMBLED_TX_BITS", "UE_PHY_UL_PAYLOAD_TX_BITS"

supported_oai_tracer_messages = {
    # gNB messages
    "GNB_PHY_UL_FD_PUSCH_IQ": {
        "file_name_prefix": "rx-fd-data",
        "scope": "gNB",
        "description": "Frequency-domain RX data",
        "serialization_scheme": ["subcarriers", "ofdm_symbols"],
    },
    "GNB_PHY_UL_FD_DMRS": {
        "file_name_prefix": "tx-pilots-fd-data",
        "scope": "gNB",
        "description": "Frequency-domain TX PUSCH DMRS data",
        "serialization_scheme": ["subcarriers", "ofdm_symbols"],
    },
    "GNB_PHY_UL_FD_CHAN_EST_DMRS_POS": {
        "file_name_prefix": "raw-ce-fd-data",
        "scope": "gNB",
        "description": "Frequency-domain raw channel estimates (at DMRS positions)",
        "serialization_scheme": ["subcarriers", "ofdm_symbols"],
    },
    "GNB_PHY_UL_FD_CHAN_EST_DMRS_INTERPL": {
        "file_name_prefix": "raw-inter-ce-fd-data",
        "scope": "gNB",
        "description": "Interpolcated Frequency-domain raw channel estimates",
        "serialization_scheme": ["subcarriers", "ofdm_symbols"],
    },
    "GNB_PHY_UL_PAYLOAD_RX_BITS": {
        "file_name_prefix": "rx-payload-bits",
        "scope": "gNB",
        "description": "Received PUSCH payload bits",
        "serialization_scheme": ["bits", "subcarriers", "ofdm_symbols"],
    },
    # UE messages
    "UE_PHY_UL_SCRAMBLED_TX_BITS": {
        "file_name_prefix": "tx-scrambled-bits",
        "scope": "UE",
        "description": "Transmitted scrambled PUSCH bits",
        "serialization_scheme": ["bits", "subcarriers", "ofdm_symbols"],
    },
    "UE_PHY_UL_PAYLOAD_TX_BITS": {
        "file_name_prefix": "tx-payload-bits",
        "scope": "UE",
        "description": "Transmitted PUSCH payload bits",
        "serialization_scheme": ["bits", "subcarriers", "ofdm_symbols"],
    },
}
# -------------------------------------------
# System configuration: gNB
project_id_gnb = 2335
read_shm_path_gnb = "/tmp/gnb_app1"
write_shm_path_gnb = "/tmp/gnb_app2"

# System configuration: UE
project_id_ue = 2336
read_shm_path_ue = "/tmp/ue_app1"
write_shm_path_ue = "/tmp/ue_app2"


# initialize shared memory
def attach_shm(shm_path, project_id):
    key = ipc.ftok(shm_path, project_id)
    shm = ipc.SharedMemory(key, 0, 0)
    # I found if we do not attach ourselves
    # it will attach as ReadOnly.
    shm.attach(0, 0)
    return shm


def detach_shm(shm):
    try:
        shm.detach()
        print("Shared memory detached successfully.")
    except ipc.ExistentialError:
        print("Shared memory segment does not exist.")


def remove_shm(shm):
    try:
        shm.remove()
        print("Shared memory removed successfully.")
    except ipc.ExistentialError:
        print("Shared memory segment does not exist.")


# check data if avalible in the shared memory
def is_data_available_in_memory(shm, bufIdx, general_message_header_length, timeout=20):
    start_time = time.time()
    while True:
        buf = shm.read(bufIdx + general_message_header_length)
        n_bytes = sum(buf)
        print("Data Recording App: Waiting for Measurements!")
        if n_bytes > 0:
            print("There is data in memory, n_bytes: ", n_bytes)
            return True
        if (time.time() - start_time) > timeout:
            break
        time.sleep(1)
    return False


# Read data from Shared memory based Data Conversion Service message structure
def read_data_from_shm(shm, bufIdx, tracer_msgs_identities):
    # print buffer index
    if DEBUG_BUFFER_READING:
        print("Buffer Index: ", bufIdx)
    # get general message header list
    general_msg_header_list, general_message_header_length = data_recording_messages_def.get_general_msg_header_list()
    buf = shm.read(bufIdx + general_message_header_length)
    n_bytes = sum(buf)
    if n_bytes == 0:
        raise Exception('ERROR: No data available in memory')
    msg_id = struct.unpack('<H', buf[bufIdx:bufIdx+general_msg_header_list.get("msg_id")])[0]
    bufIdx += general_msg_header_list.get("msg_id")
    frame = struct.unpack('<H', buf[bufIdx:bufIdx+general_msg_header_list.get("frame")])[0]
    bufIdx += general_msg_header_list.get("frame")
    slot = struct.unpack('B', buf[bufIdx:bufIdx+general_msg_header_list.get("slot")])[0]
    bufIdx += general_msg_header_list.get("slot")
    # get time stamp:  yyyy mm dd hh mm ss msec
    nr_trace_time_stamp_yyymmdd = \
        struct.unpack('<i', buf[bufIdx:bufIdx+general_msg_header_list.get("datetime_yyyymmdd")])[0]
    bufIdx += general_msg_header_list.get("datetime_yyyymmdd")
    nr_trace_time_stamp_hhmmssmmm = \
        struct.unpack('<i', buf[bufIdx:bufIdx+general_msg_header_list.get("datetime_hhmmssmmm")])[0]
    bufIdx += general_msg_header_list.get("datetime_hhmmssmmm")
    time_stamp_milli_sec = str(nr_trace_time_stamp_yyymmdd)+"_"+str(nr_trace_time_stamp_hhmmssmmm)
    # get frame type
    frame_type = struct.unpack('B', buf[bufIdx:bufIdx+general_msg_header_list.get("frame_type")])[0]
    bufIdx += general_msg_header_list.get("frame_type")
    # get frequency range
    freq_range = struct.unpack('B', buf[bufIdx:bufIdx+general_msg_header_list.get("freq_range")])[0]
    bufIdx += general_msg_header_list.get("freq_range")
    # get subcarrier spacing
    subcarrier_spacing = struct.unpack('B', buf[bufIdx:bufIdx+general_msg_header_list.get("subcarrier_spacing")])[0]
    bufIdx += general_msg_header_list.get("subcarrier_spacing")
    # get cyclic prefix
    cyclic_prefix = struct.unpack('B', buf[bufIdx:bufIdx+general_msg_header_list.get("cyclic_prefix")])[0]
    bufIdx += general_msg_header_list.get("cyclic_prefix")
    # get symbols per slot
    symbols_per_slot = struct.unpack('B', buf[bufIdx:bufIdx+general_msg_header_list.get("symbols_per_slot")])[0]
    bufIdx += general_msg_header_list.get("symbols_per_slot")
    # get Nid cell
    Nid_cell = struct.unpack('<H', buf[bufIdx:bufIdx+general_msg_header_list.get("Nid_cell")])[0]
    bufIdx += general_msg_header_list.get("Nid_cell")
    # get rnti
    rnti = struct.unpack('<H', buf[bufIdx:bufIdx+general_msg_header_list.get("rnti")])[0]
    bufIdx += general_msg_header_list.get("rnti")
    # get rb size
    rb_size = struct.unpack('<H', buf[bufIdx:bufIdx+general_msg_header_list.get("rb_size")])[0]
    bufIdx += general_msg_header_list.get("rb_size")
    # get rb start
    rb_start = struct.unpack('<H', buf[bufIdx:bufIdx+general_msg_header_list.get("rb_start")])[0]
    bufIdx += general_msg_header_list.get("rb_start")
    # get start symbol index
    start_symbol_index = struct.unpack('B', buf[bufIdx:bufIdx+general_msg_header_list.get("start_symbol_index")])[0]
    bufIdx += general_msg_header_list.get("start_symbol_index")
    # get number of symbols
    nr_of_symbols = struct.unpack('B', buf[bufIdx:bufIdx+general_msg_header_list.get("nr_of_symbols")])[0]
    bufIdx += general_msg_header_list.get("nr_of_symbols")
    # get qam modulation order
    qam_mod_order = struct.unpack('B', buf[bufIdx:bufIdx+general_msg_header_list.get("qam_mod_order")])[0]
    bufIdx += general_msg_header_list.get("qam_mod_order")
    # get mcs index
    mcs_index = struct.unpack('B', buf[bufIdx:bufIdx+general_msg_header_list.get("mcs_index")])[0]
    bufIdx += general_msg_header_list.get("mcs_index")
    # get mcs table
    mcs_table = struct.unpack('B', buf[bufIdx:bufIdx+general_msg_header_list.get("mcs_table")])[0]
    bufIdx += general_msg_header_list.get("mcs_table")
    # get number of layers
    nrOfLayers = struct.unpack('B', buf[bufIdx:bufIdx+general_msg_header_list.get("nrOfLayers")])[0]
    bufIdx += general_msg_header_list.get("nrOfLayers")
    # get transform precoding
    transform_precoding = struct.unpack('B', buf[bufIdx:bufIdx+general_msg_header_list.get("transform_precoding")])[0]
    bufIdx += general_msg_header_list.get("transform_precoding")
    # get dmrs config type
    dmrs_config_type = struct.unpack('B', buf[bufIdx:bufIdx+general_msg_header_list.get("dmrs_config_type")])[0]
    bufIdx += general_msg_header_list.get("dmrs_config_type")
    # get ul dmrs symb pos
    ul_dmrs_symb_pos = struct.unpack('<H', buf[bufIdx:bufIdx+general_msg_header_list.get("ul_dmrs_symb_pos")])[0]
    bufIdx += general_msg_header_list.get("ul_dmrs_symb_pos")
    # get number dmrs symbols
    number_dmrs_symbols = struct.unpack('B', buf[bufIdx:bufIdx+general_msg_header_list.get("number_dmrs_symbols")])[0]
    bufIdx += general_msg_header_list.get("number_dmrs_symbols")
    # get dmrs port
    dmrs_port = struct.unpack('<H', buf[bufIdx:bufIdx+general_msg_header_list.get("dmrs_port")])[0]
    bufIdx += general_msg_header_list.get("dmrs_port")
    # get dmrs scid
    dmrs_scid = struct.unpack('<H', buf[bufIdx:bufIdx+general_msg_header_list.get("dmrs_scid")])[0]
    bufIdx += general_msg_header_list.get("dmrs_scid")
    # get nb antennas rx for gNB or nb antennas tx for UE
    nb_antennas = struct.unpack('B', buf[bufIdx:bufIdx+general_msg_header_list.get("nb_antennas")])[0]
    bufIdx += general_msg_header_list.get("nb_antennas")
    # get number of bits
    number_of_bits = struct.unpack('<I', buf[bufIdx:bufIdx+general_msg_header_list.get("number_of_bits")])[0]
    bufIdx += general_msg_header_list.get("number_of_bits")
    # get length of bytes
    length_bytes = struct.unpack('<I', buf[bufIdx:bufIdx+general_msg_header_list.get("length_bytes")])[0]
    bufIdx += general_msg_header_list.get("length_bytes")

    # print all captured data
    if DEBUG_WIRELESS_RECORDED_DATA:
        print(" ")
        print(f"Time stamp: {time_stamp_milli_sec}")
        print(f"MSG ID: {msg_id:<5} MSG Name: {tracer_msgs_identities[msg_id]}")
        print(f"Frame: {frame:<5} Slot: {slot:<5}")
        print(f"Frame Type: {frame_type:<5} Frequency Range: {freq_range:<5}"
              f"Subcarrier Spacing: {subcarrier_spacing:<5} Cyclic Prefix: {cyclic_prefix:<5} "
              f"Symbols per Slot: {symbols_per_slot:<5}")
        print(f"Nid Cell: {Nid_cell:<5} RNTI: {rnti:<5}")
        print(f"RB Size: {rb_size:<5} RB Start: {rb_start:<5} Start Symbol Index: {start_symbol_index:<5} "
              f"Number of Symbols: {nr_of_symbols:<5}")
        print(f"QAM Modulation Order: {qam_mod_order:<5} MCS Index: {mcs_index:<5} "
              f"MCS Table: {mcs_table:<5}")
        print(f"Number of Layers: {nrOfLayers:<5} Transform Precoding: {transform_precoding:<5}")
        print(f"DMRS Config Type: {dmrs_config_type:<5} UL DMRS Symbol Position: {ul_dmrs_symb_pos:<5} "
              f"Number of DMRS Symbols: {number_dmrs_symbols:<5}")
        print(f"DMRS Port: {dmrs_port:<5} DMRS SCID: {dmrs_scid:<5} "
              f"Number of Antennas: {nb_antennas:<5}")
        print(f"Number of bits: {number_of_bits:<5} Length of bytes: {length_bytes:<5}")
    # raise exception if time stamp is zero, it means that the data is not recorded yet
    if nr_trace_time_stamp_yyymmdd == 0 and nr_trace_time_stamp_hhmmssmmm == 0:
        raise Exception("ERROR: Time stamp is zero, data is not recorded yet or something wrong, check logs!")
    # get recorded data
    buf = shm.read(bufIdx + length_bytes)
    # bit_msg_index = get_index_of_id(tracer_msgs_identities, "GNB_PHY_UL_PAYLOAD_RX_BITS")
    captured_data = {}
    # If message is bit message, store data in bytes
    # then the field number_of_bits should be not zero
    if "_BITS" in tracer_msgs_identities[msg_id]:
        # recorded_data = buf[bufIdx:bufIdx + length_bytes]
        recorded_data = struct.unpack("<" + int(length_bytes) * 'B', buf[bufIdx:bufIdx + length_bytes])
        bufIdx += length_bytes
        # convert data in bytes to bits
        bits_vector = []
        for byte in recorded_data:
            bits_vector.extend([int(bit) for bit in format(int(byte), '08b')])
        captured_data["sigmf_data_type"] = "ri8_le"
        # convert to uint8
        captured_data["recorded_data"] = np.asarray(bits_vector).astype(np.uint8)
        # recorded_data_formated = recorded_data.astype(np.complex64) # convert to complex64
    else:
        recorded_data = struct.unpack("<" + int(length_bytes/2) * 'h', buf[bufIdx:bufIdx + length_bytes])
        bufIdx += length_bytes
        # print("IQ data I/Q: ", recorded_data)
        # Convert real data to complext data
        # converting list to array
        recorded_data = np.asarray(recorded_data)
        # recorded_data_complex = recorded_data
        recorded_data_complex = common_utils.real_to_complex(recorded_data)
        captured_data["sigmf_data_type"] = "cf32_le"
        # convert to complex64
        captured_data["recorded_data"] = recorded_data_complex.astype(np.complex64)
    # print("Recorded Data: ", captured_data["recorded_data"])
    # store data in dictonary
    captured_data["message_id"] = msg_id
    captured_data["message_type"] = tracer_msgs_identities[msg_id]
    captured_data["frame"] = frame
    captured_data["slot"] = slot
    captured_data["time_stamp"] = time_stamp_milli_sec
    captured_data["frame_type"] = frame_type
    captured_data["freq_range"] = freq_range
    captured_data["subcarrier_spacing"] = subcarrier_spacing
    captured_data["cyclic_prefix"] = cyclic_prefix
    # captured_data["symbols_per_slot"] = symbols_per_slot  ... not used
    captured_data["Nid_cell"] = Nid_cell
    captured_data["rnti"] = rnti
    captured_data["rb_size"] = rb_size
    captured_data["rb_start"] = rb_start
    captured_data["start_symbol_index"] = start_symbol_index
    captured_data["nr_of_symbols"] = nr_of_symbols
    captured_data["qam_mod_order"] = qam_mod_order
    captured_data["mcs_index"] = mcs_index
    captured_data["mcs_table"] = mcs_table
    captured_data["nrOfLayers"] = nrOfLayers
    captured_data["transform_precoding"] = transform_precoding
    captured_data["dmrs_config_type"] = dmrs_config_type
    captured_data["ul_dmrs_symb_pos"] = ul_dmrs_symb_pos
    captured_data["number_dmrs_symbols"] = number_dmrs_symbols
    captured_data["dmrs_port"] = dmrs_port
    captured_data["dmrs_scid"] = dmrs_scid
    captured_data["nb_antennas"] = nb_antennas
    captured_data["number_of_bits"] = number_of_bits
    return captured_data, bufIdx


# Synchronize data between gNB and UE
def sync_data_conversion_service(
        shm_reading_gnb, shm_reading_ue, sync_info, config_meta_data, gnb_args, ue_args):
    # Initialize variables
    record_idx = 0
    prev_frame = -1
    prev_slot = -1
    ue_bufIdx = 0
    gnb_bufIdx = 0

    gnb_args.num_requested_tracer_msgs = len(
        config_meta_data["data_recording_config"]["base_station"][
            "requested_tracer_messages"])
    ue_args.num_requested_tracer_msgs = len(
        config_meta_data["data_recording_config"]["user_equipment"][
            "requested_tracer_messages"])
    tracer_msgs_identities = config_meta_data["data_recording_config"][
        "tracer_msgs_identities"]

    global_info = config_meta_data["data_recording_config"]["global_info"]

    # Get UE data based on the sync data
    bufIdx = 0
    timeout_sync = time.time() + 5  # 5 seconds if no sync data found, stop the process
    while True:
        # wait for the next record
        # To do: check if we need to add exta waiting times between different events in case of
        # data streaming via network such as on UE side or gNB side
        time.sleep(0.0035)  # 2.3 ms = latency of T tracer to capture data from the RAN
        ue_bufIdx = bufIdx
        captured_data, bufIdx = read_data_from_shm(shm_reading_ue, bufIdx, tracer_msgs_identities)
        if (captured_data["frame"] == sync_info["frame"]
                and captured_data["slot"] == sync_info["slot"]):
            break
        if time.time() > timeout_sync:
            raise Exception(
                "ERROR: Data Recording NO Sync Found, check Tracer Services if they are connected!")

    # Get gNB data based on the sync data
    bufIdx = 0
    while True:
        time.sleep(0.0035)
        gnb_bufIdx = bufIdx
        captured_data, bufIdx = read_data_from_shm(
            shm_reading_gnb, bufIdx, tracer_msgs_identities)
        if (captured_data["frame"] == sync_info["frame"]
                and captured_data["slot"] == sync_info["slot"]):
            break

    # Read Synchronized data between gNB and UE
    while True:  # read all records
        print("\nRecord number: ", record_idx)
        if DEBUG_BUFFER_READING:
            print(f"Buffer Index gNB: {gnb_bufIdx}, Buffer Index UE: {ue_bufIdx}")
        # wait for the next record
        # To do: check if we need to add exta waiting times between different events in case of
        # data streaming via network such as on UE side or gNB side
        time.sleep(0.0035)  # 2.3 ms = latency of T tracer to capture data from the RAN

        collected_metafiles = []
        # Read data from gNB T-tracer Application
        for idx in range(gnb_args.num_requested_tracer_msgs):
            time.sleep(0.0015)
            if DEBUG_WIRELESS_RECORDED_DATA:
                print(f"\nRecord number: {record_idx}, Reading MSG data ", idx)
            captured_data, gnb_bufIdx = read_data_from_shm(
                shm_reading_gnb, gnb_bufIdx, tracer_msgs_identities)
            # drive the collection file time stamp from the first message per record
            if idx == 0:
                # Get time stamp
                time_stamp_ms, time_stamp_ms_file_name = (
                    sigmf_interface.time_stamp_formating(
                        captured_data["time_stamp"], global_info["datetime_offset"]))
                global_info["collection_file"] = (
                    global_info["collection_file_prefix"]
                    + "-rec-"
                    + str(record_idx)
                    + "-"
                    + str(time_stamp_ms_file_name)
                )
                global_info["timestamp"] = time_stamp_ms

            # Write data into files with the given format
            if config_meta_data["data_recording_config"]["enable_saving_tracer_messages_sigmf"]:
                collected_metafiles.append(
                    sigmf_interface.write_recorded_data_to_sigmf(
                        captured_data, config_meta_data, global_info, record_idx))

        # Read data from UE T-tracer Application
        for idx in range(ue_args.num_requested_tracer_msgs):
            time.sleep(0.0015)
            if DEBUG_WIRELESS_RECORDED_DATA:
                print(f"\nRecord number: {record_idx}, Reading MSG data ", idx)
            captured_data, ue_bufIdx = read_data_from_shm(
                shm_reading_ue, ue_bufIdx, tracer_msgs_identities)

            # Write data into files with the given format
            if config_meta_data["data_recording_config"]["enable_saving_tracer_messages_sigmf"]:
                collected_metafiles.append(
                    sigmf_interface.write_recorded_data_to_sigmf(
                        captured_data, config_meta_data, global_info, record_idx))

        # generate SigMF collection file
        if config_meta_data["data_recording_config"]["enable_saving_tracer_messages_sigmf"]:
            data_storage_path = config_meta_data["data_recording_config"][
                "data_storage_path"]
            description = global_info["description"]
            sigmf_interface.save_sigmf_collection(
                collected_metafiles, global_info, description, data_storage_path)

        frame = captured_data["frame"]
        slot = captured_data["slot"]

        # Check for changes in frame or slot
        if frame != prev_frame or slot != prev_slot:
            record_idx += 1
            # We have reached the end of the data. Break the loop
            if record_idx >= config_meta_data["data_recording_config"]["num_records"]:
                break
        # Update previous frame and slot
        prev_frame = frame
        prev_slot = slot


# data conversion service
def data_conversion_service(shm_reading, config_meta_data, args, sync_info, do_sync):
    # Initialize variables
    record_idx = 0
    prev_frame = -1
    prev_slot = -1
    bufIdx = 0
    # Read data from T-tracer Application
    print("Data Conversion Service: Reading data from T-tracer Application")
    print("Requested Tracer Messages: ", args.num_requested_tracer_msgs)
    if args.num_requested_tracer_msgs > 0:
        num_requested_tracer_msgs = args.num_requested_tracer_msgs
    else:
        raise Exception("ERROR: No requested tracer messages found!")

    tracer_msgs_identities = config_meta_data["data_recording_config"][
        "tracer_msgs_identities"]
    global_info = config_meta_data["data_recording_config"]["global_info"]

    if do_sync:
        print(" Find Memory Index of NR MSGs based on Sync info")
        while True:
            time.sleep(0.0035)
            station_bufIdx = bufIdx
            captured_data, bufIdx = read_data_from_shm(
                shm_reading, bufIdx, tracer_msgs_identities)
            print("*Sync Status: NR Captured Data (Frame, slot): (", captured_data["frame"],
                  ", ", captured_data["slot"], "), Sync Info (Frame, slot): (", sync_info["frame"],
                  ", ", sync_info["slot"]," )")
            if (captured_data["frame"] == sync_info["frame"]
                    and captured_data["slot"] == sync_info["slot"]):
                print("*sync Pass")
                break
            print("*sync Fail")
        bufIdx = station_bufIdx

    # Read data from T-tracer Application
    while True:  # read all records
        print("\nRecord number: ", record_idx)
        if DEBUG_BUFFER_READING:
            print(f"Buffer Index: {bufIdx}")
        # wait for the next record
        # To do: check if we need to add exta waiting times between different events in case of
        # data streaming via network such as on UE side or gNB side
        time.sleep(0.0035)  # 2.3 ms = latency of T tracer to capture data from the RAN

        collected_metafiles = []
        for idx in range(num_requested_tracer_msgs):
            time.sleep(0.0015)
            if DEBUG_WIRELESS_RECORDED_DATA:
                print(f"\nRecord number: {record_idx}, Reading MSG data ", idx)
            captured_data, bufIdx = read_data_from_shm(
                shm_reading, bufIdx, tracer_msgs_identities)
            # derive the collection file time stamp from the first message per record
            if idx == 0:
                # Get time stamp
                time_stamp_ms, time_stamp_ms_file_name = (
                    sigmf_interface.time_stamp_formating(
                        captured_data["time_stamp"], global_info["datetime_offset"]))
                global_info["collection_file"] = (
                    global_info["collection_file_prefix"]
                    + "-rec-"
                    + str(record_idx)
                    + "-"
                    + str(time_stamp_ms_file_name))
                global_info["timestamp"] = time_stamp_ms
            # Write data into files with the given format
            if config_meta_data["data_recording_config"]["enable_saving_tracer_messages_sigmf"]:
                collected_metafiles.append(
                    sigmf_interface.write_recorded_data_to_sigmf(
                        captured_data, config_meta_data, global_info, record_idx))

        frame = captured_data["frame"]
        slot = captured_data["slot"]

        # generate SigMF collection file
        if config_meta_data["data_recording_config"]["enable_saving_tracer_messages_sigmf"]:
            data_storage_path = config_meta_data["data_recording_config"][
                "data_storage_path"]
            description = global_info["description"]
            sigmf_interface.save_sigmf_collection(
                collected_metafiles, global_info, description, data_storage_path)

        # Check for changes in frame or slot
        if frame != prev_frame or slot != prev_slot:
            record_idx += 1
            # We have reached the end of the data. Break the loop
            if record_idx >= config_meta_data["data_recording_config"]["num_records"]:
                break
        # Update previous frame and slot
        prev_frame = frame
        prev_slot = slot


# Write Tracer Control Message
def write_shm(shm, args):
    # Note: Big Endian >, Little Endian <
    # Note: unsigned char B, signed char b, short h, int I, long long q
    # Note: float f, double d, string s,  char c, bool ?
    # 1: config
    # 2: record
    # 3: quit
    print("Write Shared Memory: ", args.action)
    if args.action == "config":
        # Determine the length of the IP address
        ip_length = len(args.bytes_IPaddress) + 1  # String terminator
        # Construct the format string dynamically
        format_string = f"{ip_length}s"
        shm.write(
            # Config action
            struct.pack("<B", 1) +
            struct.pack("<B", ip_length) +
            struct.pack(format_string, args.bytes_IPaddress) +
            struct.pack("<h", int(args.port))
        )
        print("T-Tracer Config IP: ", args.bytes_IPaddress, " port: ", args.port)
    elif args.action == "record":
        shm.write(
                # Record action
                struct.pack("<B", int(2)) +
                struct.pack("<B", args.num_requested_tracer_msgs) +
                struct.pack(
                    "<{}h".format(len(args.req_tracer_msgs_indices)),
                    *args.req_tracer_msgs_indices,) +
                struct.pack("<I", args.num_records) +
                struct.pack("<h", args.start_frame_number)
        )
        print("T-Tracer Record: N Messags: ", args.num_requested_tracer_msgs,
              ", Msg IDs: ", args.req_tracer_msgs_indices,
              ", Num records: ", args.num_records,
              ", Start Frame: ", args.start_frame_number,
              )
    elif args.action == "quit":
        shm.write(struct.pack("<B", int(3)))  # Quit action
        print("T-Tracer Quit")
    else:
        print("Unknown action for data recording system!")


# write shared memory task
def write_shm_task(barrier, shm_id, args):
    # Wait for threads to be ready
    barrier.wait()
    # send request to related T-Tracer Application
    write_shm(shm_id, args)


if __name__ == "__main__":
    # -------------------------------------------
    # ------------- Configuration --------------
    # ------------------------------------------
    # Data Recording Configuration
    data_recording_config_file = "config/config_data_recording.json"
    # -------------------------------------------
    # Configuration
    # -------------------------------------------
    # First: get the configuration mode either local or remote
    # Second: get data recording configuration
    # Read and parse the JSON file
    with open(data_recording_config_file, "r") as file:
        config_meta_data = json.load(file)

    # get Configuration parameters
    config_meta_data, gnb_args, ue_args = config_interface.get_data_recording_config(config_meta_data)

    # check if lists of requested tracer messages IDs are not empty
    if not gnb_args.requested_tracer_messages and \
            not ue_args.requested_tracer_messages:
        raise Exception("ERROR: No requested tracer messages are provided")

    # check if gnb_requested_tracer_messages is not empty, attach to the shared memory
    if gnb_args.requested_tracer_messages:
        # attach to the shared memory
        shm_writing_gnb = attach_shm(write_shm_path_gnb, project_id_gnb)
        shm_reading_gnb = attach_shm(read_shm_path_gnb, project_id_gnb)

    # check if ue_requested_tracer_messages is not empty, attach to the shared memory
    if ue_args.requested_tracer_messages:
        # attach to the shared memory
        shm_writing_ue = attach_shm(write_shm_path_ue, project_id_ue)
        shm_reading_ue = attach_shm(read_shm_path_ue, project_id_ue)

    # get general message header list
    general_msg_header_list, general_message_header_length = \
        data_recording_messages_def.get_general_msg_header_list()

    # Add supported OAI Tracer Messages
    config_meta_data["data_recording_config"]["supported_oai_tracer_messages"] = supported_oai_tracer_messages
    # Add global info
    config_meta_data["data_recording_config"]["global_info"] = global_info

    # -------------------------------------------
    # Initialization
    # -------------------------------------------
    # -------------------------------------------
    # send Tracer Control Message request to T-Tracers Apps
    # -------------------------------------------
    #   It consists of the following fields:
    #       Config action
    #       IP address length
    #       IP address
    #       Port Number
    # check if gnb_requested_tracer_messages is not empty, config T-Tracer gNB via shared memory
    if gnb_args.requested_tracer_messages:
        # Config T-Tracer via shared memory
        gnb_args.action = "config"
        write_shm(shm_writing_gnb, gnb_args)

    # check if ue_requested_tracer_messages is not empty, config T-Tracer UE via shared memory
    if ue_args.requested_tracer_messages:
        # Config T-Tracer via shared memory
        ue_args.action = "config"
        write_shm(shm_writing_ue, ue_args)

    time.sleep(0.5)  # wait for the config to be applied

    # -------------------------------------------
    # Execution
    # -------------------------------------------
    # send Tracer Control Message request to T-Tracers Apps
    # -------------------------------------------
    #   It consists of the following fields:
    #       Record action
    #       Number of requested Tracer Messages
    #       Requested Tracer Messages ID 1, …, ID N
    #       Number of records to be recorded in slots
    #       Start SFN: Frame Index to start data collection from it, useful for future
    #       data sync between gNB and UE but not yet used
    print("Args:")
    if gnb_args.requested_tracer_messages:
        gnb_args.action = "record"
        print("gnb_args: ", gnb_args)
    if ue_args.requested_tracer_messages:
        ue_args.action = "record"
        print("ue_args: ", ue_args)

    start_time = time.time()
    print("Send data logging request us:", datetime.now().strftime("%Y%m%d-%H%M%S%f"))

    # if requested: gNB and UE Tracer Messages
    # gNB + UE Tracer Messages
    if gnb_args.requested_tracer_messages and ue_args.requested_tracer_messages:
        # Create a barrier to synchronize the threads
        barrier = threading.Barrier(2)
        with concurrent.futures.ThreadPoolExecutor() as executor:
            tracer_ue = executor.submit(write_shm_task, barrier, shm_writing_ue, ue_args)
            tracer_gnb = executor.submit(write_shm_task, barrier, shm_writing_gnb, gnb_args)
            # Wait for both functions to complete
            concurrent.futures.wait([tracer_ue, tracer_gnb])

    # gNB Tracer Messages
    elif gnb_args.requested_tracer_messages:
        write_shm(shm_writing_gnb, gnb_args)

    # UE Tracer Messages
    elif ue_args.requested_tracer_messages:
        write_shm(shm_writing_ue, ue_args)
    else:
        raise Exception("ERROR: No requested tracer messages IDs are provided")

    # -------------------------------------------
    # Read data from gNB and UE T-tracer Application
    # -------------------------------------------
    # Check if data is available in memory
    # Initialize variables
    bufIdx = 0
    timeout = 10  # 10 seconds from now

    # If gNB MSGs are requested
    if gnb_args.requested_tracer_messages:
        # Check if data is available in gNB memory
        is_gnb_data_in_memory = is_data_available_in_memory(
            shm_reading_gnb, bufIdx, general_message_header_length, timeout)
        # Report the status of gNB T-Tracer APP locally
        if not is_gnb_data_in_memory:
            print("Error: gNB: Check t-Tracer APP of gNB, check IPs and Ports")
            print("Error: gNB: If IPs and Ports are correct, re-run the hanging app.")
            print("It seems the socket was not closed properly")
            raise Exception("ERROR: Time out, check if gNB T-Tracer APP connected to stack")
    # If UE MSGs are requested
    if ue_args.requested_tracer_messages:
        # Check if data is available in UE memory
        is_ue_data_in_memory = is_data_available_in_memory(
            shm_reading_ue, bufIdx, general_message_header_length, timeout)
        # Report the status of UE T-Tracer APP locally
        if not is_ue_data_in_memory:
            print("Error: UE: Check t-Tracer APP of UE, check IPs and Ports")
            print("Error: UE: If IPs and Ports are correct, re-run the hanging app.")
            print("It seems the socket was not closed properly")
            raise Exception("ERROR: Time out, check if UE T-Tracer APP connected to stack")

    # -------------------------------------------
    # Sync data between gNB and UE
    # -------------------------------------------
    # write JSON file
    common_utils.write_config_data_recording_app_json(config_meta_data)
    sync_info = {}
    if gnb_args.requested_tracer_messages and ue_args.requested_tracer_messages:
        # Sync data between gNB and UE
        sync_info = sync_service.sync_gnb_ue_captured_data(shm_reading_gnb, shm_reading_ue)
        print("\n***Sync data between gNB and UE: ", sync_info)
        # Read data from gNB and UE T-tracer Applications
        sync_data_conversion_service(
            shm_reading_gnb, shm_reading_ue, sync_info, config_meta_data, gnb_args, ue_args)
    elif gnb_args.requested_tracer_messages:
        # Read data from gNB T-tracer Application
        data_conversion_service(
            shm_reading_gnb, config_meta_data, gnb_args, sync_info, do_sync=False)
    elif ue_args.requested_tracer_messages:
        # Read data from UE T-tracer Application
        data_conversion_service(
            shm_reading_ue, config_meta_data, ue_args, sync_info, do_sync=False)
    else:
        raise Exception("ERROR: No requested tracer messages IDs are provided")

    # measure Elapsed time
    time_elapsed = time.time() - start_time
    time_elapsed_ms = int(time_elapsed * 1000)
    print(
        "Elapsed time of getting Requested Messages and writing data and meta data files:",
        colored(time_elapsed_ms, "yellow"), "ms",)

    # Stop T-Tracer Application function
    if gnb_args.requested_tracer_messages:
        gnb_args.action = "quit"
        write_shm(shm_writing_gnb, gnb_args)
        # Add Sleep time to ensure that the message sent to the UE T-tracer application is received
        # before the shared memory is detached
        time.sleep(0.5)
        # Clean shared memory
        detach_shm(shm_reading_gnb)
        detach_shm(shm_writing_gnb)
        remove_shm(shm_reading_gnb)
        remove_shm(shm_writing_gnb)
    if ue_args.requested_tracer_messages:
        ue_args.action = "quit"
        write_shm(shm_writing_ue, ue_args)
        # Add Sleep time to ensure that the message sent to the UE T-tracer application is received
        # before the shared memory is detached
        time.sleep(0.5)
        # Clean shared memory
        detach_shm(shm_reading_ue)
        detach_shm(shm_writing_ue)
        remove_shm(shm_reading_ue)
        remove_shm(shm_writing_ue)

    print("End of the RF Data Recording API")
    pass


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
# file common/utils/data_recording/lib/sync_service.py
# brief Sync service between captured Data from 5GNR gNB and UE
# author Abdo Gaber
# date 2025
# version 1.0
# company Emerson, NI Test and Measurement
# email:
# note
# warning

import struct
from lib import data_recording_messages_def

DEBUG_POW_MEAS_SYNC = True


# check if first frame ahead:
def is_frame_ahead(frame1, frame2, max_frame=1023):
    """
    Check if frame1 is ahead of frame2, considering wrap-around from max_frame to 0.

    Args:
        frame1 (int): The first frame number.
        frame2 (int): The second frame number.
        max_frame (int): The maximum frame number before wrap-around. Default is 1023.

    Returns:
        bool: True if frame1 is ahead of frame2, False otherwise.
    """
    # Calculate the difference considering wrap-around
    diff = (frame1 - frame2 + (max_frame + 1)) % (max_frame + 1)
    # If the difference is less than half the range, frame1 is ahead
    return diff < (max_frame + 1) // 2


# find the frame and slot start
def find_frame_slot_start(dataset1_start, dataset2_start):
    """
    Function to find the frame and slot start for data sync
    Args:
    dataset1_start_info: Dictionary containing the start information of dataset1
    dataset2_start_info: Dictionary containing the start information of dataset2
    Returns:
    sync_info: Dictionary containing the sync information
        sync_info["frame"] = frame_start
        sync_info["slot"]  = slot_start

    """
    sync_info = {}

    if dataset1_start["frame"] == dataset2_start["frame"]:
        frame_diff = 0
        frame_start = dataset1_start["frame"]
        slot_start = max(dataset1_start["slot"], dataset2_start["slot"])

    elif is_frame_ahead(dataset1_start["frame"], dataset2_start["frame"]):
        frame_start = dataset1_start["frame"]
        slot_start = dataset1_start["slot"]
        frame_diff = (dataset1_start["frame"]- dataset2_start["frame"] + 1024) % 1024
    elif is_frame_ahead(dataset2_start["frame"], dataset1_start["frame"]):
        frame_start = dataset2_start["frame"]
        slot_start = dataset2_start["slot"]
        frame_diff = (dataset2_start["frame"] - dataset1_start["frame"] + 1024) % 1024
    # check first if the delta time between the two datasets is larger than expected offset time.
    # So, the sync will not be applied
    # check during the calculation of the frame the ramp-around from 1023 to 0
    if abs(frame_diff) > 6:
        raise Exception(
            f"Frame difference between dataset1 and dataset2 is too large: {frame_diff}. "
            "This may indicate a problem with the data collection or synchronization."
        )

    # Determine the starting frame and slot for data sync
    sync_info["frame"] = frame_start
    sync_info["slot"] = slot_start
    return sync_info


# Read data from gNB T-tracer Application
def get_frame_slot_start(shm_reading, bufIdx, general_msg_header_list,
                         general_message_header_length):
        buf = shm_reading.read(bufIdx + general_message_header_length)
        msg_id = struct.unpack("<H", buf[bufIdx: bufIdx + general_msg_header_list.get("msg_id")])[0]
        bufIdx += general_msg_header_list.get("msg_id")
        frame = struct.unpack("<H", buf[bufIdx: bufIdx + general_msg_header_list.get("frame")])[0]
        bufIdx += general_msg_header_list.get("frame")
        slot = struct.unpack("B", buf[bufIdx: bufIdx + general_msg_header_list.get("slot")])[0]
        return frame, slot


# Sync data between gNB and UE
def sync_gnb_ue_captured_data(shm_reading_gnb, shm_reading_ue):
    """
    Function to get the sync (frame, slot) data between gNB and UE
    Args:
    shm_reading_gnb: Shared memory for gNB
    shm_reading_ue: Shared memory for UE
    Returns:
    sync_info: Dictionary containing the sync information between gNB and UE
        sync_info["frame"] = frame_start
        sync_info["slot"]  = slot_start
    """
    # get general message header list
    general_msg_header_list, general_message_header_length = (
        data_recording_messages_def.get_general_msg_header_list())

    # Read data from gNB T-tracer Application
    bufIdx = 0
    frame_gnb, slot_gnb = get_frame_slot_start(
        shm_reading_gnb, bufIdx, general_msg_header_list, general_message_header_length)
    # Read data from UE T-tracer Application
    bufIdx = 0
    frame_ue, slot_ue = get_frame_slot_start(
        shm_reading_ue, bufIdx, general_msg_header_list, general_message_header_length)
    dataset1_start = {}
    dataset1_start["frame"] = frame_gnb
    dataset1_start["slot"] = slot_gnb
    dataset2_start = {}
    dataset2_start["frame"] = frame_ue
    dataset2_start["slot"] = slot_ue
    # Sync data between gNB and UE
    # We noticed that the maximum difference between the frame number of gNB and UE is 3 frames
    # Calculate the frame difference considering the wrap-around from 1023 to 0
    sync_info = find_frame_slot_start(dataset1_start, dataset2_start)
    print(" gNB Start info: ", dataset1_start)
    print(" UE Start info: ", dataset2_start)
    print(" gNB and UE Sync info: ", sync_info)
    return sync_info

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
# file common/utils/data_recording/lib/config_interface.py
# brief Data Recording App Configuration Interface
# author Abdo Gaber
# date 2025
# version 1.0
# company Emerson, NI Test and Measurement
# email:
# note
# warning

import yaml
import json
import argparse


# Function to read the main configuration file in JSON format
def read_main_config_file_json(main_config_file: str) -> dict:
    """
    Reads main config file.
    """
    # read general parameter set from yaml config file
    with open(main_config_file, "r") as file:
        main_config = json.load(file)

    return main_config


# Function to read the main configuration file in YAML format
def read_main_config_file(main_config_file: str) -> dict:
    """
    Reads main config file.
    """
    # read general parameter set from yaml config file
    with open(main_config_file, "r") as file:
        main_config = yaml.load(file, Loader=yaml.Loader)

    return main_config


# Function to parse the OAI T_messages file and get the index of a given string
def parse_message_file(file_path):
    with open(file_path, "r") as file:
        content = file.readlines()
    # Extract lines that start with 'ID' and remove the 'ID = ' prefix
    tracer_msgs_identities = [
        line.strip().replace("ID = ", "")
        for line in content
        if line.strip().startswith("ID")
    ]

    return tracer_msgs_identities


# Function to get the index of a given string in the list of ID lines
def get_index_of_id(tracer_msgs_identities, message_id):
    try:
        return tracer_msgs_identities.index(message_id)
    except ValueError:
        return -1  # Return -1 if the string is not found


# Function to get the indices of requested tracer messages
def get_requested_tracer_msgs_indices(requested_tracer_messages, tracer_msgs_identities):
    # get requested tracer messages indices
    req_tracer_msgs_indices = []
    for idx, value in enumerate(requested_tracer_messages):
        msg_index = get_index_of_id(tracer_msgs_identities, value)
        req_tracer_msgs_indices.append(msg_index)
    print("Requested Traces IDs: ", req_tracer_msgs_indices)
    return req_tracer_msgs_indices


# Function to get the data recording configuration
def get_data_recording_config(config_meta_data):
    parser = argparse.ArgumentParser(description="request messages IDs")
    ue_args = parser.parse_args()
    gnb_args = parser.parse_args()

    # get Tracer Messages IDs from the T-Tracer Messages file
    #   MSG list order should be similar to the T-Tracer Messages txt file
    #   Be sure that you are using the same T_messages file that is used in the OAI project
    #   OAI Path: .../common/utils/T/T_messages.txt

    # Parse the T_messages file
    tracer_msgs_identities = parse_message_file(
        config_meta_data["data_recording_config"]["t_tracer_message_definition_file"])
    config_meta_data["data_recording_config"]["tracer_msgs_identities"] = tracer_msgs_identities

    # get requested tracer messages indices for gNB
    gnb_args.requested_tracer_messages = \
        config_meta_data["data_recording_config"]["base_station"]["requested_tracer_messages"]
    # get requested tracer messages indices for UE
    ue_args.requested_tracer_messages = \
        config_meta_data["data_recording_config"]["user_equipment"]["requested_tracer_messages"]

    # check if gnb_requested_tracer_messages is not empty
    if gnb_args.requested_tracer_messages:
        config_meta_data["data_recording_config"]["base_station"][
            "req_tracer_msgs_indices"] = get_requested_tracer_msgs_indices(
            gnb_args.requested_tracer_messages, tracer_msgs_identities)

        # get gNB Trace Messages
        gnb_args.num_records = config_meta_data["data_recording_config"]["num_records"]
        gnb_args.start_frame_number = config_meta_data["data_recording_config"]["start_frame_number"]
        gnb_args.req_tracer_msgs_indices = \
            config_meta_data["data_recording_config"]["base_station"]["req_tracer_msgs_indices"]
        gnb_args.num_requested_tracer_msgs = len(gnb_args.req_tracer_msgs_indices)
        # Split the string into IP and port
        gnb_args.IPaddress, gnb_args.port = config_meta_data["data_recording_config"][
            "tracer_service_baseStation_address"].split(":")
        gnb_args.bytes_IPaddress = bytes(gnb_args.IPaddress, "utf-8")

    # check if ue_requested_tracer_messages is not empty
    if ue_args.requested_tracer_messages:
        config_meta_data["data_recording_config"]["user_equipment"][
            "req_tracer_msgs_indices"] = get_requested_tracer_msgs_indices(
            ue_args.requested_tracer_messages, tracer_msgs_identities)

        # get UE Trace Messages
        ue_args.num_records = config_meta_data["data_recording_config"]["num_records"]
        ue_args.start_frame_number = config_meta_data["data_recording_config"]["start_frame_number"]
        ue_args.req_tracer_msgs_indices = \
            config_meta_data["data_recording_config"]["user_equipment"]["req_tracer_msgs_indices"]
        ue_args.num_requested_tracer_msgs = len(ue_args.req_tracer_msgs_indices)
        ue_args.IPaddress, ue_args.port = config_meta_data["data_recording_config"][
            "tracer_service_userEquipment_address"].split(":")
        ue_args.bytes_IPaddress = bytes(ue_args.IPaddress, "utf-8")

    return config_meta_data, gnb_args, ue_args

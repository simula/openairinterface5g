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
# file common/utils/data_recording/lib/data_recording_messages_def.py
# brief defination of captured data recording messages
# author Abdo Gaber
# date 2025
# version 1.0
# company Emerson, NI Test and Measurement
# email:
# note
# warning


# Data Collection Trace Messages - General message structure - number of bytes
def get_general_msg_header_list():
    """
    shared memory layout written from the app:
    =================================
    msg_id                  (uint8)  message type ID
    frame                   (uint16)
    slot                    (uint8)
    datetime_yyyymmdd       (uint32)
    datetime_hhmmssmmm      (uint32)
    frame_type              (uint8)
    freq_range              (uint8)
    subcarrier_spacing      (uint8)
    cyclic_prefix           (uint8)
    symbols_per_slot        (uint8)
    Nid_cell                (uint16)
    rnti                    (uint16)
    rb_size                 (uint16)
    rb_start                (uint16)
    start_symbol_index      (uint8)
    nr_of_symbols           (uint8)
    qam_mod_order           (uint8)
    mcs_index               (uint8)
    mcs_table               (uint8)
    nrOfLayers              (uint8)
    transform_precoding     (uint8)
    dmrs_config_type        (uint8)
    ul_dmrs_symb_pos        (uint16)
    number_dmrs_symbols     (uint8)
    dmrs_port               (uint16)
    dmrs_scid               (uint16)
    nb_antennas             (uint8)
    number_of_bits          (uint32)
    length_bytes            (uint32)
    For IQ Data: IQ samples: I0, Q0, I1, Q1, ... I_x, Q_x (int16)
    For bit data: bits: b0, b1, b2, ... b_x (uint8)

    """
    # Data Collection Trace Messages - General message structure - number of bytes
    general_msg_header_list = {
        "msg_id": 2,
        "frame": 2,
        "slot": 1,
        "datetime_yyyymmdd": 4,
        "datetime_hhmmssmmm": 4,
        "frame_type": 1,
        "freq_range": 1,
        "subcarrier_spacing": 1,
        "cyclic_prefix": 1,
        "symbols_per_slot": 1,
        "Nid_cell": 2,
        "rnti": 2,
        "rb_size": 2,
        "rb_start": 2,
        "start_symbol_index": 1,
        "nr_of_symbols": 1,
        "qam_mod_order": 1,
        "mcs_index": 1,
        "mcs_table": 1,
        "nrOfLayers": 1,
        "transform_precoding": 1,
        "dmrs_config_type": 1,
        "ul_dmrs_symb_pos": 2,
        "number_dmrs_symbols": 1,
        "dmrs_port": 2,
        "dmrs_scid": 2,
        "nb_antennas": 1,  # for gNB or nb_antennas_tx for UE
        "number_of_bits": 4,
        "length_bytes": 4,
    }
    # initial number of bytes to read to get data
    general_message_header_length = 0
    for key, value in general_msg_header_list.items():
        general_message_header_length = general_message_header_length + value
    return general_msg_header_list, general_message_header_length

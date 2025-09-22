This document contains documentation for the 5G RRC layer, destined towards
developers. It explains the basic working of the RRC, and various UE procedure
schemes (connection, reestablishment, handover) including their interworking
with other layers.

User documentation, such as general configuration options, are described in [a
separate page](./rrc-usage.md).

[[_TOC_]]

# General

5G RRC is basically an ITTI message queue with associated handlers. It
sequentially reads received ITTI messages and handles them through the function
`rrc_gnb_task()` (constituing the thread entry point function). In this
function, one can see three main groups of messages in a big switch statement:

- NGAP (messages starting with `NGAP_`)
- F1AP (messages starting with `F1AP_`)
- E1AP (messages starting with `E1AP_`)

For each message, a corresponding handler is called. The messages are roughly
named according to the 3GPP specification messages, so it should already be
possible to find the message in the switch based on a message name in the spec.

Note that RRC is inherently single-threaded, and processes messages in a FIFO
order.

# Sequence Diagrams of UE procedures

The following section presents a number of common UE procedures for connection
establishment&control, bearer establishment, etc. The intention is to help
developers find specific functions within RRC in the context of these
procedures. For more information on message handlers at F1 and E1 layers,
please refer to the respective [F1 documentation](../F1AP/F1-design.md) and [E1
documentation](../E1AP/E1-design.md).

For more information on these procedures, please also refer to 3GPP TS 38.401
(NG-RAN Architecture Description) and O-RAN.WG5.C.1-v11 (NR C-plane profile).

A lot of the following diagrams would show an exchange between CU-CP, DU, and
UE, in which the CU-CP forwards an "RRC Message" to the UE via an F1AP DL RRC
Message Transfer through the DU, and correspondingly receives the "RRC Message
Answer", as follows:

```mermaid
sequenceDiagram
  participant ue as UE
  participant du as DU
  participant cucp as CU-CP
  cucp->>du: F1AP DL RRC Msg Transfer (RRC Message)
  du->>ue: RRC Message
  ue->>du: RRC Message Answer
  du->>cucp: F1AP UL RRC Msg Transfer (RRC Message Answer)
```

To better utilize horizontal space, these exchanges have been collapsed as
follows, but should be read as the above exchange:

```mermaid
sequenceDiagram
  participant ue as UE
  participant du as DU
  participant cucp as CU-CP
  cucp->>ue: F1AP DL RRC Msg Transfer (RRC Message)
  ue->>cucp: F1AP UL RRC Msg Transfer (RRC Message Answer)
```

## Initial connection setup/Registration

This sequence diagram shows the principal steps for an initial UE connection.
This can either happen through a _Registration Request_ (e.g., UE connects
"from flight mode"), or a _Service Request_ (i.e., UE connects after leaving
coverage). Both requests are handled similarly by the gNB, and basically
distinguish themselves, from the point of view of the gNB, by having PDU
sessions in the NGAP Initial Context Setup Request present (Service Request) or
not (Registration Request, with following PDU Session Resource Setup Request
procedure).

```mermaid
sequenceDiagram
  participant ue as UE
  participant du as DU
  participant cucp as CU-CP
  participant cuup as CU-UP
  participant amf as AMF

  ue->>du: Msg1/Preamble
  du->>ue: Msg2/RAR
  ue->>cucp: F1AP Initial UL RRC Msg Tr (RRC Setup Req, in Msg3)
  Note over cucp: rrc_handle_RRCSetupRequest()
  cucp->>ue: F1AP DL RRC Msg Transfer (RRC Setup, in Msg4)
  ue->>cucp: F1AP UL RRC Msg Transfer (RRC Setup Complete)
  Note over cucp: rrc_handle_RRCSetupComplete()
%% TODO: when is RRC connected?
  cucp->>amf: NGAP Initial UE Message (NAS Registration/Service Req)
  Note over amf,ue: NAS Authentication Procedure (see 24.501)
  Note over amf,ue: NAS Security Procedure (see 24.501)
  amf->>cucp: NGAP Initial Context Setup Req
  Note over cucp: rrc_gNB_process_NGAP_INITIAL_CONTEXT_SETUP_REQ()
  Note over cucp: rrc_gNB_generate_SecurityModeCommand()
  cucp->>ue: F1AP DL RRC Msg Transfer (RRC Security Mode Command)
  ue->>cucp: F1AP UL RRC Msg Transfer (RRC Security Mode Complete)
  Note over cucp: rrc_gNB_decode_dcch() (inline)
  opt No UE Capabilities present
    Note over cucp: rrc_gNB_generate_UECapabilityEnquiry()
    cucp->>ue: F1AP DL RRC Msg Transfer (RRC UE Capability Enquiry)
    ue->>cucp: F1AP UL RRC Msg Transfer (RRC UE Capability Information)
    Note over cucp: handle_ueCapabilityInformation()
  end
  opt PDU Sessions in NGAP Initial Context Setup Req present
    Note over cucp: trigger_bearer_setup()
    cucp->>cuup: E1AP Bearer Context Setup Req
    cuup->>cucp: E1AP Bearer Context Setup Resp
    Note over cucp: rrc_gNB_process_e1_bearer_context_setup_resp()
    cucp->>du: F1AP UE Context Setup Req
    du->>cucp: F1AP UE Context Setup Resp
    Note over cucp: rrc_CU_process_ue_context_setup_response()
    Note over cucp: e1_send_bearer_updates()
    cucp->>cuup: E1AP Bearer Context Modification Req
    cucp->>ue: F1AP DL RRC Msg Transfer (RRC Reconfiguration)
    cuup->>cucp: E1AP Bearer Context Modification Resp
    ue->>cucp: F1AP UL RRC Msg Transfer (RRC Reconfiguration Complete)
    Note over cucp: handle_rrcReconfigurationComplete()
  end
  Note over cucp: rrc_gNB_send_NGAP_INITIAL_CONTEXT_SETUP_RESP()
  cucp->>amf: NGAP Initial Context Setup Resp (NAS Registration/Service Complete)
```

Note that if no PDU session is present in the NGAP Initial UE Context Setup
Req, no F1AP UE Context Setup will be observed during this initial phase.

If there is no PDU session set up during NGAP Initial Context Setup Req, the UE
typically requests a PDU session(s) through a NAS procedure, which is basically
the same code paths as the above optional PDU Session setup during an NGAP
Initial Context Setup procedure:

```mermaid
sequenceDiagram
  participant ue as UE
  participant du as DU
  participant cucp as CU-CP
  participant cuup as CU-UP
  participant amf as AMF

  ue->>cucp: F1AP UL RRC Msg Transfer (RRC UL Information Transfer)
  cucp->>amf: NGAP UL NAS Transport (NAS PDU Session Establishment Req)
  amf->>cucp: NGAP PDU Session Resource Setup Req (NAS PDU Session Establishment Accept)
  Note over cucp: rrc_gNB_process_NGAP_PDUSESSION_SETUP_REQ()
    Note over cucp: trigger_bearer_setup()
    cucp->>cuup: E1AP Bearer Context Setup Req
    cuup->>cucp: E1AP Bearer Context Setup Resp
  Note over cucp: rrc_gNB_process_e1_bearer_context_setup_resp()
  cucp->>du: F1AP UE Context Setup Req
  du->>cucp: F1AP UE Context Setup Resp
  Note over cucp: rrc_CU_process_ue_context_setup_response()
    Note over cucp: e1_send_bearer_updates()
    cucp->>cuup: E1AP Bearer Context Modification Req
  cucp->>ue: F1AP DL RRC Msg Transfer (RRC Reconfiguration + NAS PDU Session Establishment Accept)
    cuup->>cucp: E1AP Bearer Context Modification Resp
  ue->>cucp: F1AP UL RRC Msg Transfer (RRC Reconfiguration Complete)
  Note over cucp: handle_rrcReconfigurationComplete()
  Note over cucp: rrc_gNB_send_NGAP_PDUSESSION_SETUP_RESP()
  cucp->>amf: NGAP PDU Session Resource Setup Resp
```

## Reestablishment

The following sequence diagram shows the principal steps during a
reestablishment request. When handling the RRC Reestablishment Request at the
CU-CP, it notably checks if this UE is already known, if the UE is still at the
same DU as it used to be previously (might change because of, or despite
handover), and other validation checks. If all checks pass, the CU-CP continues
with the reestablishment procedure; if any check fails, the CU-CP tries to
release the old UE at the AMF, and continues with the "normal" connection setup
(registration/service request) described above.

```mermaid
sequenceDiagram
  participant ue as UE
  participant du as DU
  participant cucp as CU-CP
  participant cuup as CU-UP
  participant amf as AMF

  ue->>du: Msg1/Preamble
  du->>ue: Msg2/RAR
  ue->>du: Msg3 w/ RRC Reestabishment Request (old C-RNTI/PCI)
  Note over du: DU creates UE context for new RNTI
  du->>cucp: F1AP Initial UL RRC Msg Tr (RRC Reestab Req, in Msg3)
  Note over cucp: rrc_handle_RRCReestablishmentRequest()
  alt UE known and at the original DU
      Note over cucp: rrc_gNB_generate_RRCReestablishment()<br />SRB1 PDCP reestablishment
      cucp->>du: F1AP DL RRC Msg Tr (old gNB-DU-UE ID)
      Note over du: Reuse configuration of and delete old UE<br />(in dl_rrc_message_transfer())
      du->>ue: RRC Reestablishment
      ue->>du: RRC Reestablishment Complete
      du->>cucp: F1AP UL RRC Msg Tr (RRC Reestab Complete)
      Note over cucp: handle_rrcReestablishmentComplete()
      Note over cucp: cuup_notify_reestablishment()
        cucp->>cuup: E1AP Bearer Context Modification Req
      cucp->>ue: F1AP DL RRC Msg Transfer (RRC Reconfiguration)
        cuup->>cucp: E1AP Bearer Context Modification Resp
      ue->>cucp: F1AP UL RRC Msg Transfer (RRC Reconfiguration Complete)
      Note over cucp: handle_rrcReestablishmentComplete()
  else Fallback*
      cucp->>amf: NGAP UE Context Release Request
      Note over cucp: rrc_gNB_generate_RRCSetup()
      Note over ue,amf: Continue with connection setup (registration/service request)
  end
```

## Inter-DU Handover (F1)

The basic handover (HO) structure is as follows. In order to support various
handover "message passing implementation" (F1AP, NGAP, XnAP), RRC employs
callbacks to signal HO Accept (`ho_req_ack()`), HO Success (`ho_success()`),
and HO Cancel (`ho_cancel()`). These can be used to trigger the corresponding
functionality based on mentioned "message passing implementation".

The following sequence diagram shows the basic functional execution of a
successful handover in the case of an F1 handover. Note the callbacks as
mentioned above:

```mermaid
sequenceDiagram
  participant ue as UE
  participant sdu as source DU
  participant tdu as target DU
  participant cucp as CU-CP
  participant cuup as CU-UP

  Note over ue,sdu: UE active on source DU
  alt HO triggered through A3 event
    ue->>sdu: RRC Measurement Report
    sdu->>cucp: F1AP UL RRC Msg Transfer (RRC Measurement Report)
    Note over cucp: Handover decision (A3 event trigger)
  else Manual Trigger
    Note over cucp: Handover decision (e.g., telnet)
  end
  Note over cucp: nr_rrc_trigger_f1_ho() ("on source CU")
  Note over cucp: nr_initiate_handover() ("on target CU")
  cucp->>tdu: F1AP UE Context Setup Req
  Note over tdu: Create UE context
  tdu->>cucp: F1AP UE Context Setup Resp (incl. CellGroupConfig)
  Note over cucp: rrc_CU_process_ue_context_setup_response() ("on target CU")
    Note over cucp: cuup_notify_reestablishment()
    cucp->>cuup: E1AP Bearer Context Modification Req
  cucp-->>cucp: callback: ho_req_ack()
  Note over cucp: nr_rrc_f1_ho_acknowledge() ("on source CU")
  cucp->>sdu: F1AP Context Modification Req (RRC Reconfiguration)
    cuup->>cucp: E1AP Bearer Context Modification Resp
  sdu->>ue: RRC Reconfiguration
  sdu->>cucp: F1AP Context Modification Resp
  Note over sdu: Stop scheduling UE
  Note over ue: Resync
  Note over ue,tdu: RA (Msg1 + Msg2)
  ue->>tdu: RRC Reconfiguration Complete
  tdu->>cucp: F1AP UL RRC Msg Transfer (RRC Reconfiguration Complete)
  Note over cucp: handle_rrcReconfigurationComplete() ("on target CU")
  cucp-->>cucp: callback: ho_success()
  Note over cucp: nr_rrc_f1_ho_complete() ("on source CU")
  cucp->>sdu: F1AP UE Context Release Command
  sdu->>cucp: F1AP UE Context Release Complete
  Note over ue,tdu: UE active on target DU
```
## Inter-gNB Handover (N2)

This is an inter-NG-RAN procedure. The N2 handover specification is defined in the following documents:

* 3GPP TS 23.502, 4.9.1.3 Inter NG-RAN node N2 based handover:
  - Outlines detailed handover signaling flows for N2-based handovers.
  - Covers both intra-system (between 5G gNBs) and inter-system (between 5G and LTE eNBs) handovers.
* 3GPP TS 38.300, section 9 Mobility and State Transitions:
  - describes mobility procedures at the NG-RAN level, depending on the RRC state.
* 3GPP TS 38.413 (NGAP), section 8.4 UE Mobility Management Procedures:
  - Specifies the signaling procedures over the N2 interface.
  - Includes messages like Handover Request, Handover Command, and Handover Preparation.
* 3GPP TS 38.331 (RRC): details the UE-level RRC procedures involved during handovers

### End-to-end flow

```mermaid
sequenceDiagram
  participant ue as UE
  participant sdu as source DU
  participant scucp as source CU-CP
  participant scuup as source CU-UP
  participant tdu as target DU
  participant tcucp as target CU-CP
  participant tcuup as target CU-UP
  participant amf as AMF

  Note over ue,sdu: UE active on source DU
  alt HO triggered through A3 event
    ue->>sdu: RRC Measurement Report
    sdu->>scucp: F1AP UL RRC Msg Transfer (RRC Measurement Report)
    Note over scucp: Handover decision (A3 event trigger)
  else Manual Trigger
    Note over scucp: Handover decision (e.g., telnet)
  end
  Note over scucp: nr_rrc_trigger_n2_ho() ("on source CU")
  scucp->>amf: HANDOVER REQUIRED
  amf->>tcucp: HANDOVER REQUEST
  Note over tcucp: rrc_gNB_process_Handover_Request
  Note over tcucp: trigger_bearer_setup
  tcucp->>tcuup: Bearer Context Setup Request
  tcuup->>tcucp: Bearer Context Setup Response
  Note over tcucp: rrc_gNB_process_e1_bearer_context_setup_resp
  Note over tcucp: nr_rrc_trigger_n2_ho_target() ("on target CU")
  Note over tcucp: nr_initiate_handover()
  tcucp->>tdu: F1AP UE Context Setup Req
  Note over tdu: Create UE context
  tdu->>tcucp: F1AP UE Context Setup Resp (incl. CellGroupConfig)
  Note over tcucp: rrc_CU_process_ue_context_setup_response() ("on target CU")
  Note over tcucp: e1_send_bearer_updates()
  tcucp->>tcuup: E1AP Bearer Context Modification Req
  tcuup->>tcucp: E1AP Bearer Context Modification Resp
  tcucp-->>tcucp: callback: ho_req_ack()
  Note over tcucp: nr_rrc_n2_ho_acknowledge() ("on target CU")
  tcucp->>amf: HANDOVER REQUEST ACKNOWLEDGE (data forwarding info)
  amf->>scucp: HANDOVER COMMAND
  scucp->>sdu: F1AP UE Context Modification Req (RRC Reconfiguration)
  sdu->>ue: RRC Reconfiguration
  sdu->>scucp: F1AP UE Context Modification Resp
  Note over sdu: Stop scheduling UE
  Note over scucp: rrc_CU_process_ue_context_modification_response()
  Note over scucp: e1_send_bearer_updates()
  scucp->>scuup: E1 Bearer Context Modification Req (PDCP Status Request)
  scuup->>scucp: E1 Bearer Context Modification Resp (PDCP Status Info)
  scucp->>amf: NG Uplink RAN Status Transfer
  amf->>tcucp: NG Downlink RAN Status Transfer
  tcucp->>tcuup: E1 Bearer Context Modification Req (PDCP Status Info)
  tcuup->>tcucp: E1 Bearer Context Modification Resp
  Note over ue: UE attachment to target DU
  Note over ue,tdu: RA (Msg1 + Msg2)
  ue->>tdu: RRC Reconfiguration Complete
  tdu->>tcucp: F1AP UL RRC Msg Transfer (RRC Reconfiguration Complete)
  tcucp-->>tcucp: callback: ho_success()
  Note over tcucp: nr_rrc_n2_ho_complete() ("on target CU")
  Note over tcucp: handle_rrcReconfigurationComplete() ("on target CU")
  tcucp->>amf: HANDOVER NOTIFY
  amf->>scucp: UE Context Release Command
  note over scucp: ngap_gNB_handle_ue_context_release_command
  note over scucp: rrc_gNB_process_NGAP_UE_CONTEXT_RELEASE_COMMAND
  scucp->>scuup: E1 Bearer Context Release Command
  scuup->>scucp: E1 Bearer Context Release Complete
  Note over scucp: rrc_gNB_generate_RRCRelease
  scucp->>sdu: F1 UE Context Release Command
  sdu->>scucp: F1 UE Context Release Complete
  note over scucp: rrc_CU_process_ue_context_release_complete
  note over scucp: rrc_remove_ue
  Note over ue,tdu: UE active on target DU
```

# Structures

## Cells

OAI 5G RRC does not actually handle multiple cells as of now, but multiple DUs,
each being limited to one cell.

Cell-related data is stored in `nr_rrc_du_container_t`, and kept in a tree
indexed by the SCTP association ID.

## CU-UPs

CU-UP information is stored in `nr_rrc_cuup_container_t`, and kept in a tree
indexed by the SCTP association ID.

## Transactions

The RRC keeps track of ongoing transaction (RRC procedures) through a per-UE
array `xids`, which is indexed with a transaction ID `xid` in `[0,3]` to keep
track of ongoing transactions. Upon RRC Reconfiguration Complete, these IDs are
deleted.

Note that while 38.331 requires only one RRC transaction to happen at a time,
the 5G RRC does not actually ensure this; it only tries to prevent it by
chaining of transactions and some checks. For instance, the Security Mode
procedure is followed by a UE capability procedure, and some procedures are not
executed if others are ongoing (ex.: handover if there is reconfiguration).
However, it might be possible to trigger a procedure while another is ongoing.

As of now, no queueing mechanims exists to ensure only one operation is
ongoing, which would likely also simplify the code.

## Handover

Handover-related data is stored in a per-UE structure of type
`nr_handover_context_t`. It is a pointer and only set during handover
operation. This data structure has in turn two pointers, one to the source CU
(`nr_ho_source_cu_t`), and one to the target CU (`nr_ho_target_cu_t`). In F1,
both are present. In N2 and Xn, only one pointer is supposed to be set at the
corresponding CU.

`nr_ho_source_cu_t` contains notably a function pointer `ho_cancel` for
handover cancel.  `nr_ho_target_cu_t` contains function pointers `ho_req_ack`
for handover request acknowledge, `ho_success` for handover success,
`ho_failure` for handover failure (N2 only).
be seen in the sequence diagram above, either the "target CU" or "source CU"
needs to do an operation, and a "switch" from target to source CU is done using
these function pointers. For instance, in F1, the handover request acknowledge
function pointers merely calls another (RRC) function which triggers a
reconfiguration. In the case of N2, the handover request acknowledge function
pointer should trigger the NGAP Handover Request Acknowledge, and the handover
success function pointer should trigger the NGAP Handover Success message.

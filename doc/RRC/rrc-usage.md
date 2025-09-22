This document describes the basic functioning of the 5G RRC layer, describes
the periodic output, and explains the various configuration options that
influence its behavior.

Developer documentation, such as UE connection control flow, reestablishment, or handover, are described in [a separate page](./rrc-dev.md).

[[_TOC_]]

# General

The RRC layer controls the basic connection setup of UEs as well as additional
procedures. It is the fundamental building block of OAI's CU-CP, and interacts
with lower layers (DU, basically MAC and RLC) through F1AP messages, and with
the CU-UP through E1AP messages. More information can be found in the
respective [F1AP page](../F1-design.md) and [E1AP page](../E1AP/E1-design.md).

# Periodic output and interpretation

Similarly to the scheduler, the RRC periodically prints information about
connected UEs and DUs into file `nrRRC_stats.log` in the current working
directory of the executable running the RRC (typically, `nr-softmodem`). The
output lists first all UEs that are currently connected, and then all DUs, in
order.

For each UE, it prints:

```
UE 0 CU UE ID 1 DU UE ID 40352 RNTI 9da0 random identity c0f1ac9824000000:
    last RRC activity: 5 seconds ago
    PDU session 0 ID 10 status established
    associated DU:  DU assoc ID 8
```

where `UE 0` is the UE index, CU UE ID and DU UE IDs are the IDs used to
exchange over F1 (cf. scheduler logs). Further, it shows RNTI, when the last
RRC activity happened, the status of PDU sessions and which DU is associated
(through the SCTP association ID).


For each DU, it prints:

```
1 connected DUs
[1] DU ID 3584 (gNB-OAI-DU) assoc_id 8: nrCellID 12345678, PCI 0, SSB ARFCN 641280
    TDD: band 78 ARFCN 640008 SCS 30 (kHz) PRB 106
```

i.e., an index (`[1]`), the DU ID and it's name, the SCTP association ID
(`assoc_id 8`, cf. UE information), and DU specific information for the cell
(cell ID, physical cell identity/PCI, the SSB frequency in ARFCN notation, the
band and Point A ARFCN, subcarrier spacing/SCS, and the number of resource
blocks/PRB). Only one cell per DU is supported.

As of now, it does not print information about connected CU-UPs or AMFs.

# Configuration of the RRC

## Split-related options (when running in a CU or CU-CP)

See [F1 documentation](../F1-design.md) for information about the F1 split.
See [E1 documentation](../E1AP/E1-design.md) for information about the E1 split.

## RRC-specific configuration options

In the `gNBs` section of the gNB/CU/CU-CP configuration file is the
RRC-specific configuration

### cell-specific options

Note that some SIBS are configured at the CU and some at the DU; please consult
the [MAC configuration](../MAC/mac-usage.md) as well for SIB configuration.

- `gNB_ID` and `gNB_name`: ID and name of the gNB
- `tracking_area_code`: the current tracking area code in the range `[0x0001,
  0xfffd]`
- `plmn`: the PLMN, which is a list of entries consisting of:
  - `mcc`: mobile country code
  - `mnc`: mobile network code
  - `mnc_length`: length of mobile network code, allowed values: 2, 3
  - `snssaiList`: list of NSSAI (network selection slice assistence
    information, "slice ID"), which itself consists in
    - `sst`: slice service type, in `[1,255]`
    - `sd` (default `0xffffff`): slice differentiator, in `[0,0xffffff]`,
      `0xffffff` is a reserved value and means "no SD"
    Note that: SST=1, no SD is "eMBB"; SST=2, no SD is "URLLC"; SST=3, no SD
    is "mMTC"
- `enable_sdap` (default: false): enable the use of the SDAP layer. If
  deactivated, a transparent SDAP header is prepended to packets, but no
  further processing is being done.
- `cu_sibs` (default: `[]`) list of SIBs to give to the DU for transmission.
  Currently, SIB2 is supported.

### UE-specific configuration

- `um_on_default_drb` (default: false): use RLC UM instead of RLC AM on default
  bearers
- `drbs` (default: 0): the number of DRBs to allocate for a UE, only useful for
  do-ra or phy-test testing

### Neighbor-gNB configuration

#### What is a gNB neighbor?

Network continuity is a key aspect of 5G. In the 5G architecture, gNB neighbors play a central role in maintaining service continuity through mechanisms such as handover and load balancing.  By definition, a gNB neighbor is another gNB that can be measured and linked by the UE. If the current serving gNB is no longer optimal, the UE may connect to a neighbor gNB.

To support this behavior, the network configuration specifies additional frequencies and cells that the UE should measure. The UE reports these measurements to the network, which then decides whether or not to initiate a handover.

Neighbor types include:
- **Intra-gNB neighbors** - cells belonging to the same gNB
- **Inter-gNB neighbors** - cells belonging to different gNBs
- **Inter-RAT neighbors** - cells belonging to another RAT (e.g., LTE)

#### Required configuration parameters

To define a neighbor cell in the configuration file, the following parameters are typically needed:
- **gNB ID** - unique identifier of the gNB
- **Cell ID** - identifier of the cell within the gNB
- **Physical Cell ID** - identifier of the cell’s synchronization signal (PCI)
- **Absolute Frequency** - frequency used by the SSB (absoluteFrequencySSB)
- **Subcarrier Spacing** - numerology (e.g., 15 kHz, 30 kHz)
- **PLMN configuration** - MCC, MNC, and MNC length
- **Tracking Area Code (TAC)** - identifier of the tracking area

Refer to the [handover tutorial](../handover-tutorial.md) for more information.

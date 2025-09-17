This tutorial explains how to perform handovers. For the moment, only F1
handovers are supported.

[[_TOC_]]

# Considered setup for F1 handover

We consider one CU and two DUs, connected over F1. The UE is initially
connected over the radio interface ("Uu") to DU0. Via movement to a new
position ("new pos"), it will trigger an event such that the CU triggers a
handover of the UE from DU0 to DU1. Alternatively, a manual trigger can do the
same.

![F1 Handover setup](./RRC/ho.png)

# Steps to run F1 handover with OAI UE

Measurement reporting and processing of RRC Reconfiguration for Mobility are
not completed at the UE. Nevertheless, it is possible to make simple handover
tests without any radio setup, on a single PC, with the OAI UE, in RFsimulator.

## Build with telnet support

Since the UE does not support any measurement reporting, it cannot trigger a
handover on its own; it has to be triggered manually through telnet. Thus,
build both gNB and UE as well as activate the build of telnet to that purpose:

    ./build_oai --ninja --nrUE --gNB --build-lib telnetsrv

## Run the setup

This tutorial assumes you have a core network running; [refer to the
corresponding tutorial](./NR_SA_Tutorial_OAI_CN5G.md) if this is not the case
yet.

We will use the TDD configuration files in the repository for the
[CU](../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb-cu.sa.f1.conf) as well for
[DU0](../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb-du.sa.band78.106prb.rfsim.pci0.conf)
and
[DU1](../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb-du.sa.band78.106prb.rfsim.pci1.conf).
Note how the DUs differ in their DU ID (for identification at the CU), nr
cellid (global identification), physical cell ID (identification through UE),
frequency (limitation at OAI UE), and IP address.

Make sure that using RFsimulator and the CU and each DU, you can achieve a full
connection of the UE (independently, i.e., running one DU a time). Once this is
done, follow below steps to trigger a handover:

Start the CU including telnet support:

    sudo ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb-cu.sa.f1.conf --telnetsrv --telnetsrv.shrmod ci

Start DU0:

    sudo ./nr-softmodem --rfsim -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb-du.sa.band78.106prb.rfsim.pci0.conf --rfsimulator.serveraddr 127.0.0.1

This will show an error `[HW]   connect() to 127.0.0.1:4043 failed,
errno(111)`. _This is expected_, because the RFsim server is at the UE (to be
able to serve two RFsim clients, one DU each; see below for more info). Proceed
by starting the UE, and let it connect completely (this should make the error
go away):

    sudo ./nr-uesoftmodem -C 3450720000 -r 106 --numerology 1 --ssb 516 -O <config>  --rfsim --rfsimulator.serveraddr server

Note how the RFsimulator roles have been switched, and RFsim server is at the
UE side; _this is important_. Replace `<config>` with the UE configuration
matching your core. If you followed the CN and oaiUE tutorials, you can remove
`-O <config>` and replace it with `--uicc0.imsi 001010000000001`.

Once the UE is connected, start DU1:

    sudo ./nr-softmodem --rfsim -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb-du.sa.band78.106prb.rfsim.pci1.conf --rfsimulator.serveraddr 127.0.0.1

Once DU1 is online, you can trigger a handover by issuing this command

    echo ci trigger_f1_ho | nc 127.0.0.1 9090 && echo

You should see how the UE switches from one DU to another. See additional
information further below.

A number of remarks:

1. It is important that you start DU0, UE, DU1 in order, and having UE connect
   to DU0 before starting DU1. This is because we don't employ any channel
   emulation, and the UE could not decode the SIB1 of DU0 to connect.
1. The RFsimulator roles are switched. Typically, the gNB RFsim acts as the
   server and the UE as a client. However, RFsim is limited to one server with
   multiple clients. Since the UE should be able to connect to both DUs, it has
   to act as the server, and both DUs are a client.
1. If you see errors `could not open a socket` and/or `Could not start the RF
   device`, this means that RFsim could not be started. Handover will not work;
   please refer to the preceding point to fix this (i.e., run the UE as the
   RFsim server).
1. In some cases, if the RFsim server is at the UE, the whole system can block;
   in this case, stop UE and all DUs and restart (the CU can keep running).

## Additional information to the manual HO trigger

You can trigger the handover manually by logging in through telnet:

    telnet 127.0.0.1 9090

and then manually typing `ci trigger_f1_ho`.

The command using `nc` (`netcat`) above triggers handovers directly from bash.
It does the same as logging in through telnet otherwise, and is a shorthand.

The full command is

    ci trigger_f1_ho [cu-ue-id]

`cu-ue-id` is optional. If only one UE context is present in the RRC, it will
trigger the handover for this UE in a round-robin fashion across all DUs. If
there is only one DU, the handover request will be rejected. Similarly, if
there are multiple UE contexts present at RRC, the handover will be rejected,
and you have to manually type the CU UE ID. You can see a list of all UEs in
the file `nrRRC_stats.log` that is printed periodically in the working
directory of the CU.

# Steps to run F1 handover with COTS UE

You can do handover across DUs with a COTS UE. Note that these DUs should be
separated by at least multiple meters to ensure that the UE will receive
different signal strengths when moving between cells.

We support both intra-frequency and inter-frequency handovers. We have verified with USRPs
only, although other radios should work as well.

For UEs, we verified Quectel modules and iPhones. Note, though, that not all
phones might work; for instance, we did not achieve handovers with a OnePlus
Nord, yet.

## Steps

First, make sure that you can run both DUs with the CU independently. Use the
same radio hardware for both radios to ensure that both cells can be received
equally good by the UE.

In order to enable handovers (triggered by the UE), you have to configure the
neighbour relation of the DUs at the CU. To do so, proceed as follows:

1. To simplify filling the right values in the neighbour configuration, you can
   rely on the information the CU has about both DUs. Start the CU and both
   DUs. Navigate to the directory from which you started the CU, and print RRC
   statistics:
   ```
   cat nrRRC_stats.log
   ```
1. Fill in the [`neighbour-config.conf`](../ci-scripts/conf_files/neighbour-config.conf) configuration file as shown below, and
   `@include` it in the CU file.
1. Start the CU and both DUs.
1. Bring the phone close to one cell, and leave flight mode. It should connect
   to the DU to which it is closer.
1. Move the UE towards the other DU; it should trigger an "A3 event" (Neighbour
   Becomes Better than Serving), and the CU will trigger the handover to the
   other DU.

The output on the terminal should be the same as with RFsim. If no handover is
triggered:

- Make sure that both DUs use the same hardware.
- Make sure that the UE sees both cells. For instance, you can switch to flight
  mode, go closer to the other DU, and switch off flight mode -- the UE should
  connect to that second DU.
- We did not manage handover with every phone yet -- make sure you use one of
  the list provided above.

You can also force a handover through telnet as described above. (In fact, the
decision about a handover is always at the network-side, the UE only "assists"
through measurements telling the CU that one DU is stronger than others.
Hence, "forcing" a handover just means that you manually trigger the handover,
instead of waiting for UE measurement report.)

## Example neighbour configuration

Below is an example neighbour configuration. It is based on this DU information
gathered from `nrRRC_stats.log` at the CU:

```
[1] DU ID 3585 (gNB-in-docker) assoc_id 4161: nrCellID 11111111, PCI 1, SSB ARFCN 643296
    TDD: band 78 ARFCN 642024 SCS 30 (kHz) PRB 106
[2] DU ID 3584 (gNB-in-docker) assoc_id 4163: nrCellID 12345678, PCI 0, SSB ARFCN 643296
    TDD: band 78 ARFCN 642024 SCS 30 (kHz) PRB 106
```

Note how both DUs have one cell on the same frequency and the same radio
configuration.  From this, fill the neighbour list as shown below.

Concretely, the first cell is `12345678` (on DU `[2]`), and it has `11111111`
(on DU `[1]`) as its neighbour; hence in the first block, you fill
`physical_cellId` and other values for DU `[1]`, and vice versa.

The below configuration further enables periodic measurements, A2 event
("Serving becomes worse than threshold"), and A3 events ("Neighbour Becomes
Better than Serving"). The A2 event can be disabled by setting `enable = 0`. A3
events cannot be disabled as of now. Further, the A3 events can be made
specific to cells; `cell_id = -1` means "any cell".

```
neighbour_list = (
  {
    nr_cellid = 12345678;
    neighbour_cell_configuration = (
      {
        gNB_ID = 0xe01;
        nr_cellid = 11111111;
        physical_cellId = 1;
        absoluteFrequencySSB = 643296;
        subcarrierSpacing = 1; #30 KHz
        plmn = { mcc = 001; mnc = 01; mnc_length = 2};
        tracking_area_code = 1;
      }
    )
  },
  {
    nr_cellid = 11111111;
    neighbour_cell_configuration = (
      {
        gNB_ID = 0xe00;
        nr_cellid = 12345678;
        physical_cellId = 0;
        absoluteFrequencySSB = 643296;
        subcarrierSpacing = 1; #30 KHz
        plmn = { mcc = 001; mnc = 01; mnc_length = 2};
        tracking_area_code = 1;
      }
    )
  }
 );


nr_measurement_configuration = {
  Periodical = {
    enable = 1;
    includeBeamMeasurements = 1;
    maxNrofRS_IndexesToReport = 4;
  };

  A2 = {
    enable = 1;
    threshold = 60;
    timeToTrigger = 1;
  };

  A3 = ({
    cell_id = -1; #Default
    offset = 10;
    hysteresis = 0;
    timeToTrigger = 1
  })
};
```
`@include` this configuration file inside the gNB section of CU file as shown below.

```
    plmn_list = ({ mcc = 222; mnc = 01; mnc_length = 2; snssaiList = ({ sst = 1, sd = 0xffffff })});


    @include "neighbour-config.conf"

```

# Handovers triggers and NTN

Typically, in terrestrial networks, channel measurements as well as criteria
such as load in base stations, is used to determine when and where to handover
a UE.

## NTN

Doppler spreading and time selectivity of the channel are already a challenge
for conventional terrestrial networks. However, in the context of
non-terrestrial networks (NTN), and low-earth orbit (LEO) systems, the
satellites can have speeds up 7.56 km/s, which is much faster than 0.14 km/s of
a high-speed train in terrestrial networks. Also, the delays in LEO are more varied
and longer, and the path loss is larger, because the communication distances is up
to 10 times longer than in terrestrial networks. Moreover, downlink in LEO
presents a high interference from adjacent satellite beams, and all these
features contribute to reduced received signal strength variation in these
networks compared to terrestrial networks. Typically, the criteria used in
algorithms presented in conventional terrestrial networks to trigger a handover
from a gNB to another one is based on signal strength measurements (cf., A3
event above). However, the reduced received signal strength variation in NTNs
make these algorithms inefficient for LEO systems.  Therefore, algorithms with
criteria for handover triggering that address the specifics of LEO systems are
crucial for an efficient handover processing that ensures a robust
communication with low dropping probability. Some criteria for handovers in NTN
are as follows:

- **Measurement-based triggering**: This method is based on signal strength
  measurement, and as stated above, it may not be efficient. The triggering
  thresholds and which measurement events to use as triggers, as reference
  signal received power (RSRP), reference signal received quality (RSRQ), or
  received signal strength indicator (RSSI), should be configured. This method
  relies on UE estimates and established channel estimation techniques, however
  it would require neighbouring cell lists which can be hard because the
  fast-moving of satellites leads to a fast cell coverage deviation.

- **Location-based triggering**: This method is based on UE and satellite
  location, which can be applied jointly (or not) with another trigger as the
  measurement-based trigger. For instance, for a deterministic satellite
  movement, it is possible to predict the configure triggering condition, and
  the initial association of the UE can be performed based on the distance with
  the nearest satellite, because we can know the location of the UE and NTN
  satellite to compute the distance.

- **Elevation angles of source and target cells based triggering**: This method
  is similar to the previous one, but it is based on the largest elevation
  angle.

- **Time/timer-based triggering**: This method uses triggering conditions based
  on UTC time or a timer-based solution, which can also be applied jointly (or
  not) with another trigger as the measurement-based trigger. The timer-based
  handover trigger considers the deterministic satellite movement to predict
  the time duration for which the satellite’s footprint covers a certain zone.

- **Timing advance value-based triggering**: This method uses the timing
  advance value (independently or jointly with another trigger) to trigger a
  handover to the target cell. It is appropriate to overcome the Random-Access
  preamble reception issue, where the UE needs to pre-compensate the instant
  which sends the preamble. However, UEs with GNSS support are required to
  perform this method.

## Simple location/time-based trigger

A location-based handover trigger, somewhat aligned with 3GPP Rel.17, taking
advantage of deterministic satellite movement, can be implemented whereby it is
assumed that the locations of the UE and the NTN satellite are known. From a
practical point of view, this means that we know how long it takes to perform
the handover, and therefore we only need to implement a timer.

It is possible to modify the source code to trigger a time-based handover, or
combine with other methods. If all what is required is a trigger every 15
seconds, you can also resort to the telnet-based handover trigger above, and
run in a terminal:

```bash
while true; do
  echo ci trigger_f1_ho | nc -N 127.0.0.1 9090 && echo
  sleep 15
done
```

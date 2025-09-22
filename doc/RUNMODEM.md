<table style="border-collapse: collapse; border: none;">
  <tr style="border-collapse: collapse; border: none;">
    <td style="border-collapse: collapse; border: none;">
      <a href="http://www.openairinterface.org/">
         <img src="./images/oai_final_logo.png" alt="" border=3 height=50 width=150>
         </img>
      </a>
    </td>
    <td style="border-collapse: collapse; border: none; vertical-align: center;">
      <b><font size = "5">Running OAI 5G Softmodems</font></b>
    </td>
  </tr>
</table>

This document explains some options for running 5G executables.

After you have [built the softmodem executables](BUILD.md) you can set your
default directory  to the build directory `cmake_targets/ran_build/build/` and
start testing some use cases. Below, the description of the different OAI
functionalities should help you choose the OAI configuration that suits your
need.

[[_TOC_]]

# Simulators

## RFsimulator

The RFsimulator is an OAI device replacing the radio heads (for example the
USRP device). It allows connecting the oai UE (LTE or 5G) and respectively the
oai eNodeB or gNodeB through a network interface carrying the time-domain
samples, getting rid of over the air unpredictable perturbations. This is the
ideal tool to check signal processing algorithms and protocols implementation.
The RFsimulator has some preliminary support for channel modeling.

It is planned to enhance this simulator with the following functionalities:

- Support for multiple eNodeB's or gNodeB's for hand-over tests

This is an easy use-case to setup and test, as no specific hardware is required. The [rfsimulator page](../radio/rfsimulator/README.md) contains the detailed documentation.

## L2 nFAPI Simulator

This simulator connects an eNodeB and UEs through an nFAPI interface,
short-cutting the L1 layer. The objective of this simulator is to allow multi
UEs simulation, with a large number of UEs (ideally up to 255).

As for the RFsimulator, no specific hardware is required. The [L2 nfapi
simulator page](./L2NFAPI.md) contains the detailed documentation.

# Running with a true radio head

OAI supports different radio heads, the following are tested in the CI:

1. [Monolithic eNodeB](https://gitlab.eurecom.fr/oai/openairinterface5g/wikis/HowToConnectCOTSUEwithOAIeNBNew) where the whole signal processing is performed in a single process
2. IF4P5 mode, where frequency domain samples are carried over ethernet, from the RRU which implement part of L1(FFT,IFFT,part of PRACH),  to a RAU
3. Monolithic gNodeB: see next section, or the [standalone tutorial](NR_SA_Tutorial_COTS_UE.md)


# 5G NR

## NSA setup with COTS UE

This setup requires an EPC, an OAI eNB and gNB, and a COTS Phone. A dedicated page describe the setup can be found [here](https://gitlab.eurecom.fr/oai/openairinterface5g/wikis/home/gNB-COTS-UE-testing).
The `--nsa` flag must be used to run gNB in non-standalone mode.


### Launch eNB

```bash
sudo ./lte-softmodem -O ../../../targets/PROJECTS/GENERIC-LTE-EPC/CONF/enb.band7.tm1.50prb.usrpb210.conf
```

### Launch gNB

```bash
sudo ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-LTE-EPC/CONF/gnb.band78.tm1.106PRB.usrpn300.conf --nsa
```

You should see the X2 messages in Wireshark and at the eNB.

## SA setup with OAI NR-UE

The standalone mode is the default mode. 

Before tag `2024.w45`, the default mode was NSA. Thus, in the past, to run
either the gNB or the UE in standalone mode, it was necessary to provide the
`--sa` flag in the command line. This is not the case anymore. If provided
the softmodem exits reporting that it does not know this option.

The default (SA) mode does the following:
- At the gNB:
* The RRC encodes SIB1 according to the configuration file and transmits it through NR-BCCH-DL-SCH.

- At the UE:
* Decode SIB1 and starts the 5G NR Initial Access Procedure for SA:
  1) 5G-NR RRC Connection Setup
  2) NAS Authentication and Security
  3) 5G-NR AS Security Procedure
  4) 5G-NR RRC Reconfiguration
  5) Start Downlink and Uplink Data Transfer

Command line parameters for UE in standalone mode:
- `-C` : downlink carrier frequency in Hz (default value 0)
- `--CO` : uplink frequency offset for FDD in Hz (default value 0)
- `--numerology` : numerology index (default value 1)
- `-r` : bandwidth in terms of RBs (default value 106)
- `--band` : NR band number (default value 78)
- `--ssb` : SSB start subcarrier (default value 516)

To simplify the configuration for the user testing OAI UE with OAI gNB, the latter prints the following LOG that guides the user to correctly set some of the UE command line parameters:

```shell
[PHY]   Command line parameters for OAI UE: -C 3319680000 -r 106 --numerology 1 --ssb 516
```

You can run this, using USRPs, on two separate machines:

```shell
sudo ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band78.fr1.106PRB.usrpb210.conf --gNBs.[0].min_rxtxtime 6
sudo ./nr-uesoftmodem -r 106 --numerology 1 --band 78 -C 3619200000 --ssb 516
```

With the **RFsimulator** (on the same machine), just add the option `--rfsim` to both gNB and NR UE command lines.

UE capabilities can be passed according to the [UE Capabilities](#UE-Capabilities) section.

A detailed tutorial is provided at this page [NR_SA_Tutorial_OAI_nrUE.md](./NR_SA_Tutorial_OAI_nrUE.md).

## Optional NR-UE command line options

Here are some useful command line options for the NR UE:

| Parameter                | Description                                                                                                   |
|--------------------------|---------------------------------------------------------------------------------------------------------------|
| `--ue-scan-carrier`      | Scan for cells in current bandwidth. This option can be used if the SSB position of the gNB is unknown. If multiple cells are detected, the UE will try to connect to the first cell. By default, this option is disabled and the UE attempts to only decode SSB given by `--ssb`. |
| `--ue-fo-compensation`   | Enables the initial frequency offset compensation at the UE. Useful when running over the air and/or without an external clock/time source. |
| `--cont-fo-comp`         | Enables the continuous frequency offset (FO) estimation and compensation.  Parameter value `1` specifies that the main FO contribution comes from the local oscillator's (LO) accuracy.  Parameter value `2` specifies that the main FO contribution comes from Doppler shift. |
| `--initial-fo`           | Sets the known initial frequency offset. Useful especially with large Doppler frequency, e.g. LEO satellite.  |
| `--freq-sync-P`          | Sets the coefficient for the Proportional part of the PI-controller for the continuous frequency offset compensation. Default value 0.01. |
| `--freq-sync-I`          | Sets the coefficient for the Integrating part of the PI-controller for the continuous frequency offset compensation. Default value 0.001. |
| `--ntn-initial-time-drift` | Sets the initial NTN DL time drift (feeder link and service link), given in µs/s.                           |
| `--autonomous-ta`        | Enables the autonomous TA update, based on DL drift (useful if main contribution to DL drift is movement, e.g. LEO satellite). |
| `--time-sync-P`          | Sets the coefficient for the Proportional part of the PI-controller for the time synchronization. Default value 0.5. |
| `--time-sync-I`          | Sets the coefficient for the Integrating part of the PI-controller for the time synchronization. Default value 0.0. |
| `--usrp-args`            | Equivalent to the `sdr_addrs` field in the gNB config file. Used to identify the USRP and set some basic parameters (like the clock source).  |
| `--clock-source`         | Sets the clock source (internal or external).                                                                 |
| `--time-source`          | Sets the time source (internal or external).                                                                  |

You can view all available options by typing:

```shell
./nr-uesoftmodem --help
```
## Common gNB and NR UE command line options

### Three-quarter sampling

The command line option `-E` can be used to enable three-quarter sampling for split 8 sample rate. Required for certain radios (e.g., 40MHz with B210). If used on the gNB, it is a good idea to use for the UE as well (and vice versa).

### UE Capabilities

The `--uecap_file` option can be used to pass the UE Capabilities input file (path location + filename), e.g.`--uecap_file ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/uecap_ports1.xml` for 1 layer or e.g. `--uecap_file ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/uecap_ports2.xml` for 2 layers.

This option is available for the following combinations of operation modes and gNB/nrUE softmodems:

| Mode       | Executable     | Description                                         |
|------------|----------------|-----------------------------------------------------|
| SA         | nr-uesoftmodem | Send UE capabilities from the UE to the gNB via RRC |
| phy-test   | nr-softmodem   | Mimic the reception of UE capabilities by the gNB   |
| do-ra      | nr-softmodem   | Mimic the reception of UE capabilities by the gNB   |

e.g.

```shell
sudo ./nr-uesoftmodem -r 106 --numerology 1 --band 78 -C 3319680000 --ue-nb-ant-tx 2 --ue-nb-ant-rx 2 --uecap_file /opt/oai-nr-ue/etc/uecap.xml
```

## How to run a NTN configuration

### NTN channel

A 5G NR NTN configuration only works in a non-terrestrial setup.
Therefore either SDR boards and a dedicated NTN channel emulator are required, or RFsimulator has to be configured to simulate a NTN channel.

As shown on the [rfsimulator page](../radio/rfsimulator/README.md), RFsimulator provides different possibilities.
E.g. to perform a simple simulation of a satellite in geostationary orbit (GEO), these parameters should be added to both gNB and UE command lines:
```
--rfsimulator.prop_delay 238.74
```

For simulation of a satellite in low earth orbit (LEO), two channel models have been added to rfsimulator:
- `SAT_LEO_TRANS`: transparent LEO satellite with gNB on ground
- `SAT_LEO_REGEN`: regenerative LEO satellite with gNB on board

Both channel models simulate the delay and Doppler for a circular orbit at 600 km height according to the Matlab function [dopplerShiftCircularOrbit](https://de.mathworks.com/help/satcom/ref/dopplershiftcircularorbit.html).
An example configuration to simulate a transparent LEO satellite with rfsimulator would be:
```
channelmod = {
  max_chan=10;
  modellist="modellist_rfsimu_1";
  modellist_rfsimu_1 = (
    {
      model_name     = "rfsimu_channel_enB0"
      type           = "SAT_LEO_TRANS";
      noise_power_dB = -100;
    },
    {
      model_name     = "rfsimu_channel_ue0"
      type           = "SAT_LEO_TRANS";
      noise_power_dB = -100;
    }
  );
};
```
This configuration is also provided in the file `targets/PROJECTS/GENERIC-NR-5GC/CONF/channelmod_rfsimu_LEO_satellite.conf`.

Additionally, rfsimulator has to be configured to apply the channel model.
This can be done by either providing this line in the conf file in section `rfsimulator`:
```
  options = ("chanmod");
```
Or by providing this the the command line parameters:
```
--rfsimulator.options chanmod
```

### gNB

The main parameters to cope with the large NTN propagation delay are cellSpecificKoffset, ta-Common, ta-CommonDrift and the ephemeris data (satellite position and velocity vectors).

The parameter `ntn-UlSyncValidityDuration-r17` indicates the maximum time duration from epochtime during which the UE can apply assistance information without having acquired new assistance information. i.e this enables UE to re-read SIB19 before timer with value ulSyncValidityDuration expires. The unit of the field is in seconds. Example values for GEO: 240s, MEO: 20s, LEO: 5s.
As epochtime is not implemented yet, UE starts the timer from the TTI where SIB19 is received with ntn-UlSyncValidityDuration-r17 IE.

The parameter `cellSpecificKoffset_r17` is the scheduling offset used for the timing relationships that are modified for NTN (see TS 38.213).
The unit of the field Koffset is number of slots for a given subcarrier spacing of 15 kHz.

The parameter `ta-Common-r17` is used to provide the propagation delay between the reference point (at the gNB) and the satellite.
The granularity of ta-Common is 4.072 × 10^(-3) µs. Values are given in unit of corresponding granularity.

The parameter `ta-CommonDrift-r17` indicates the drift rate of the common TA.
The granularity of ta-CommonDrift is 0.2 × 10^(-3) µs/s. Values are given in unit of corresponding granularity.

The satellite position and velocity vartors are provided using the following parameters:

`positionX-r17`, `positionY-r17`, `positionZ-r17`:
X, Y, Z coordinate of satellite position state vector in ECEF. Unit is meter.
Step of 1.3 m. Actual value = field value * 1.3.

`velocityVX-r17`, `velocityVY-r17`, `velocityVZ-r17`:
X, Y, Z coordinate of satellite velocity state vector in ECEF. Unit is meter/second.
Step of 0.06 m/s. Actual value = field value * 0.06.

These parameters can be provided to the gNB in the conf file in the section `servingCellConfigCommon`:
```
...
# GEO satellite
      cellSpecificKoffset_r17                                       = 478;
      ta-Common-r17                                                 = 58629666; # 238.74 ms
      positionX-r17                                                 = 0;
      positionY-r17                                                 = 0;
      positionZ-r17                                                 = 32433846;
      velocityVX-r17                                                = 0;
      velocityVY-r17                                                = 0;
      velocityVZ-r17                                                = 0;
# LEO satellite
#      cellSpecificKoffset_r17                                       = 40;
#      ta-Common-r17                                                 = 4634000; # 18.87 ms
#      ta-CommonDrift-r17                                            = -230000; # -46 µs/s
#      positionX-r17                                                 = 0;
#      positionY-r17                                                 = -2166908; # -2816980.4 m
#      positionZ-r17                                                 =  4910784; #  6384019.2 m
#      velocityVX-r17                                                = 0;
#      velocityVY-r17                                                = 115246; # 6914.76 m/s
#      velocityVZ-r17                                                =  50853; # 3051.18 m/s
...
```

Besides this, some timers, e.g. `sr_ProhibitTimer_v1700`, `t300`, `t301` and `t319`,  in the conf file section `gNBs.[0].TIMERS` might need to be extended for GEO satellites.
```
...
    TIMERS :
    {
      sr_ProhibitTimer       = 0;
      sr_TransMax            = 64;
      sr_ProhibitTimer_v1700 = 512;
      t300                   = 2000;
      t301                   = 2000;
      t319                   = 2000;
    };
...
```

To improve the achievable UL and DL throughput in conditions with large RTT (esp. GEO satellites), there is a feature defined in REL17 to disable HARQ feedback.
This allows to reuse HARQ processes immediately, but it breaks compatibility with UEs not supporting this REL17 feature.
To enable this feature, the `disable_harq` flag has to be added to the gNB conf file in the section `gNBs.[0]`
```
...
    sib1_tda     = 5;
    min_rxtxtime = 6;
    disable_harq = 1; // <--

    servingCellConfigCommon = (
    {
...
```

The settings for a transparent GEO satellite scenario are already provided in the file `ci-scripts/conf_files/gnb.sa.band254.u0.25prb.rfsim.ntn.conf`.
Using this conf file, an example gNB command for FDD, 5 MHz BW, 15 kHz SCS, transparent GEO satellite 5G NR NTN is this:
```
cd cmake_targets
sudo ./ran_build/build/nr-softmodem -O ../ci-scripts/conf_files/gnb.sa.band254.u0.25prb.rfsim.ntn.conf --rfsim
```

To configure NTN gNB with 32 HARQ processes in downlink and uplink, add these settings in conf files under section `gNBs.[0]`
```
...
    num_dlharq = 32;
    num_ulharq = 32;
...
```

To simulate a LEO satellite channel model with rfsimulator in UL (DL is simulated at the UE side) either the `channelmod` section as shown before has to be added to the gNB conf file, or a channelmod conf file has to be included like this:
```
@include "channelmod_rfsimu_LEO_satellite.conf"
```

The settings for a transparent LEO satellite scenario are already provided in the file `ci-scripts/conf_files/gnb.sa.band254.u0.25prb.rfsim.ntn-leo.conf`.
Using this conf file, an example gNB command for FDD, 5 MHz BW, 15 kHz SCS, trasparent LEO satellite 5G NR NTN is this:
```
cd cmake_targets
sudo ./ran_build/build/nr-softmodem -O ../ci-scripts/conf_files/gnb.sa.band254.u0.25prb.rfsim.ntn-leo.conf --rfsim
```

### NR UE

At UE side, only few parameters have to be provided, as the UE receives most relevant parameters via SIB19 from the gNB.
But to calculate the UE specific TA, the UE position has to be provided in the `ue.conf` file.
Also the LEO channel model has to be configured, e.g. by using an `@include` statement, just like on the gNB side:
```
...
position0 = {
    x = 0.0;
    y = 0.0;
    z = 6377900.0;
}

@include "channelmod_rfsimu_LEO_satellite.conf"
```

So an example NR UE command for FDD, 5MHz BW, 15 kHz SCS, transparent GEO satellite 5G NR NTN is this:
```
cd cmake_targets
sudo ./ran_build/build/nr-uesoftmodem -O ../targets/PROJECTS/GENERIC-NR-5GC/CONF/ue.conf --band 254 -C 2488400000 --CO -873500000 -r 25 --numerology 0 --ssb 60 --rfsim --rfsimulator.prop_delay 238.74
```

For LEO satellite scenarios, the parameter `--ntn-initial-time-drift` must be provided via command line, as the UE needs this value to compensate for the time drift during initial sync, before SIB19 was received.
This parameter provides the drift rate of the complete DL timing (incl. feeder link and service link) in µs/s.
Also, to perform an autonomous TA update based on the DL drift, the boolean parameter `--autonomous-ta` should be added in case of a LEO satellite scenario.

For LEO satellite scenario we assume the LO to be very accurate and the main FO contribution comes from Doppler shift.
Therefore, we use the command line parameter `--cont-fo-comp 2` to continuously compensate the DL Doppler and pre-compensate the UL Doppler.
The initial Doppler frequency offset must be provided via command line with the parameter `--initial-fo`.

For other information on optional NR UE command line options, please refer [here](#optional-nr-ue-command-line-options).

So an example NR UE command for FDD, 5MHz BW, 15 kHz SCS, transparent LEO satellite 5G NR NTN is this:
```
cd cmake_targets
sudo ./ran_build/build/nr-uesoftmodem -O ../targets/PROJECTS/GENERIC-NR-5GC/CONF/ue.conf --band 254 -C 2488400000 --CO -873500000 -r 25 --numerology 0 --ssb 60 --rfsim --rfsimulator.prop_delay 20 --rfsimulator.options chanmod --time-sync-I 0.1 --ntn-initial-time-drift -46 --autonomous-ta --initial-fo 57340 --cont-fo-comp 2
```

# Specific OAI modes

## phy-test setup with OAI UE

The OAI UE can also be used in front of a OAI gNB without the support of eNB or EPC and circumventing random access. In this case both gNB and eNB need to be run with the `--phy-test` flag. At the gNB this flag does the following
 - it reads the RRC configuration from the configuration file
 - it encodes the RRCConfiguration and the RBconfig message and stores them in the binary files `rbconfig.raw` and `reconfig.raw` in the current directory
 - the MAC uses a pre-configured allocation of PDSCH and PUSCH with randomly generated payload instead of the standard scheduler. The options `-m`, `-l`, `-t`, `-M`, `-T`, `-D`, and `-U` can be used to configure this scheduler. See `./nr-softmodem -h` for more information.

At the UE, the `--phy-test` flag will read the binary files `rbconfig.raw` and `reconfig.raw` from the current directory and process them. If you wish to provide a different path for these files, please use the options `--reconfig-file` and `--rbconfig-file`.

```bash
sudo ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-LTE-EPC/CONF/gnb.band78.tm1.106PRB.usrpn300.conf --phy-test
```

```bash
sudo ./nr-uesoftmodem --phy-test [--reconfig-file ../../../ci-scripts/rrc-files/reconfig.raw --rbconfig-file ../../../ci-scripts/rrc-files/rbconfig.raw]
```

In summary:
- If you are running on the same machine and launched the 2 executables (`nr-softmodem` and `nr-uesoftmodem`) from the same directory, nothing has to be done.
- If you launched the 2 executables from 2 different folders, just point to the location where you launched the `nr-softmodem`:
  * `sudo ./nr-uesoftmodem --rfsim --phy-test --reconfig-file /the/path/where/you/launched/nr-softmodem/reconfig-file --rbconfig-file /the/path/where/you/launched/nr-softmodem/rbconfig-file --rfsimulator.serveraddr <TARGET_GNB_INTERFACE_ADDRESS>`
- If you are not running on the same machine, you need to **COPY** the two raw files
  * `scp usera@machineA:/the/path/where/you/launched/nr-softmodem/r*config.raw userb@machineB:/the/path/where/you/will/launch/nr-uesoftmodem/`
  * Obviously this operation should be done before launching the `nr-uesoftmodem` executable.

In phy-test mode it is possible to mimic the reception of UE Capabilities at gNB through the command line parameter `--uecap_file`. Refer to the [UE Capabilities](#UE-Capabilities) section for more details.

## noS1 setup with OAI UE

Instead of randomly generated payload, in the phy-test mode we can also
inject/receive user-plane traffic over a TUN interface. This is the so-called
noS1 mode.

The noS1 mode is applicable to both gNB/UE, and enabled by passing `--noS1` as
an option. The gNB/UE will open a TUN interface which the interface names and
IP addresses `oaitun_enb1`/10.0.1.1, and `oaitun_ue1`/10.0.1.2, respectively.
You can then use these interfaces to send traffic, e.g.,
```bash
iperf -sui1 -B 10.0.1.2
```
to open an iperf server on the UE side, and
```bash
iperf -uc 10.0.1.2 -B 10.0.1.1 -i1 -t10 -b1M
```
to send data from the gNB down to the UE.

Note that this does not work if both interfaces are on the same host. We
recommend to use two different hosts, or at least network namespaces, to route
traffic through the gNB/UE tunnel.

This option is only really helpful for phy-test/do-ra (see below) modes, in
which the UE does not connect to a core network. If the UE connects to a core
network, it receives an IP address for which it automatically opens a network
interface.

## do-ra setup with OAI

The do-ra flag is used to ran the NR Random Access procedures in contention-free mode. Currently OAI implements the RACH process from Msg1 to Msg3. 

In order to run the RA, the `--do-ra` flag is needed for both the gNB and the UE.

In do-ra mode it is possible to mimic the reception of UE Capabilities at gNB through the command line parameter `--uecap_file`. Refer to the [UE Capabilities](#UE-Capabilities) section for more details.

To run using the RFsimulator:

```bash
sudo ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-LTE-EPC/CONF/gnb.band78.tm1.106PRB.usrpn300.conf --do-ra --rfsim
sudo ./nr-uesoftmodem --do-ra --rfsim --rfsimulator.serveraddr 127.0.0.1
```

Using USRPs:

```bash
sudo ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-LTE-EPC/CONF/gnb.band78.tm1.106PRB.usrpn300.conf --do-ra
```

On a separate machine:

```bash
sudo ./nr-uesoftmodem --do-ra
```


### Run OAI with SDAP & Custom DRBs

To run OAI gNB with SDAP, simply include `--gNBs.[0].enable_sdap 1` to the binary's arguments.

The DRB creation is dependent on the 5QI. 
If the 5QI corresponds to a GBR Flow it assigns a dedicated data radio bearer.
The Non-GBR flows use a shared data radio bearer.

To hardcode the DRBs for testing purposes, simply add `--gNBs.[0].drbs x` to the binary's arguements, where `x` is the number of DRBs, along with SDAP.
The hardcoded DRBs will be treated like GBR Flows. Due to code limitations at this point the max. number of DRBs is 4. 

## IF setup with OAI

OAI is also compatible with Intermediate Frequency (IF) equipment. This allows to use RF front-end that with arbitrary frequencies bands that do not comply with the standardised 3GPP NR bands. 

To configure the IF frequencies it is necessary to use two command-line options at UE side:
- `if_freq`, downlink frequency in Hz
- `if_freq_off`, uplink frequency offset in Hz

Accordingly, the following parameters must be configured in the RUs section of the gNB configuration file:
- `if_freq`
- `if_offset`

### Run OAI with custom DL/UL arbitrary frequencies

The following example uses DL frequency 2169.080 MHz and UL frequency offset -400 MHz, with a configuration file for band 66 (FDD) at gNB side.

On two separate machines with USRPs, run:

```
sudo ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-LTE-EPC/CONF/gnb.band66.tm1.106PRB.usrpx300.conf
sudo ./nr-uesoftmodem --if_freq 2169080000 --if_freq_off -400000000
```

# 5G gNB MIMO configuration

In order to enable DL-MIMO in OAI 5G softmodem, the prerequisite is to have `do_CSIRS = 1` in the configuration file. This allows the gNB to schedule CSI reference signal and to acquire from the UE CSI measurements to be able to schedule DLSCH with MIMO.

The following step is to set the number of PDSCH logical antenna ports. These needs to be larger or equal to the maximum number of MIMO layers requested (for 2-layer MIMO it is necessary to have at least two logical antenna ports).

<table style="border-collapse: collapse; border: none;">
  <tr style="border-collapse: collapse; border: none;">
    <td style="border-collapse: collapse; border: none;">
         <img src="./images/mimo_antenna_ports.png" alt="" border=3 height=100 width=300>
         </img>
    </td>
  </tr>
</table>

This image shows an example of gNB 5G MIMO logical antenna port configuration. It has to be noted that logical antenna ports might not directly correspond to physical antenna ports and each logical antenna port might consist of a sub-array of antennas.

In 5G the basic element is a dual-polarized antenna, therefore the minimal DL MIMO setup with two logical antenna ports would consist of two cross-polarized antenna elements. In a single panel configuration, as the one in the picture, this element can be repeated vertically and/or horizontally to form an equi-spaced 1D or 2D array. The values N1 and N2 represent the number of antenna ports in the two dimensions and the supported configurations are specified in Section 5.2.2.2.1 of TS 38.214.

The DL logical antenna port configuration can be selected through configuration file. `pdsch_AntennaPorts_N1` can be used to set N1 parameter, `pdsch_AntennaPorts_N2` to set N2 and `pdsch_AntennaPorts_XP` to set the cross-polarization configuration (1 for single pol, 2 for cross-pol). To be noted that if XP is 1 but N1 and/or N2 are larger than 1, this would result in a non-standard configuration and the PMI selected would be the identity matrix regardless of CSI report. The default value for each of these parameters is 1. The total number of PDSCH logical antenna ports is the multiplication of those 3 parameters.

Finally the number of TX physical antenna in the RU part of the configuration file, `nb_tx`, should be equal or larger than the total number of PDSCH logical antenna ports.

It is possible to limit the number supported DL MIMO layers via RRC configuration, e.g. to a value lower than the number of logical antenna ports configured, by using the configuration file parameter `maxMIMO_layers`.

[Example of configuration file with parameters for 2-layer MIMO](https://gitlab.eurecom.fr/oai/openairinterface5g/-/blob/develop/targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band77.fr1.273PRB.2x2.usrpn300.conf)

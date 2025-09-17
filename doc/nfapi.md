This document describes the SmallCellForum (SCF) (n)FAPI split in 5G, i.e.,
between the MAC/L2 and PHY/L1. It also describes how to make use of the 
multiple transport mechanisms between the 2.

The interested reader is recommended to read a copy of the SCF 222.10
specification ("FAPI"). This includes information on what is P5, P7, and how
FAPI works. The currently used version is SCF 222.10.02, with some messages
upgraded to SCF 222.10.04 due to bugfixes in the spec. Further information
about nFAPI can be found in SCF 225.2.0.

[[_TOC_]]

# Quickstart

Compile OAI as normal. Start the CN and make sure that the VNF configuration
matches the PLMN/IP addresses. Then, run the VNF

    sudo NFAPI_TRACE_LEVEL=info ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb-vnf.sa.band78.106prb.nfapi.conf --nfapi VNF

Afterwards, start and connect the PNF

    sudo NFAPI_TRACE_LEVEL=info ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb-pnf.band78.rfsim.conf --nfapi PNF --rfsim

Finally, you can start the UE (observe the radio configuration info in the
VNF!)

    sudo ./nr-uesoftmodem -r 106 --numerology 1 --band 78 -C 3619200000 -O ue.conf

You should not observe a difference between nFAPI split and monolithic.


# Status

All FAPI message can be transferred between VNF and PNF. This is because OAI
uses FAPI with its corresponding messages internally, whether a split is in use
or not.

The nFAPI split mode supports any radio configuration that is also supported by
the monolithic gNB, with the notable exceptions that only numerologies of 15kHz
and 30kHz (mu=0 and mu=1, respectively) are supported.

The VNF requests to be notified about every slot by the PNF. No delay
management is employed as of now; instead, the PNF sends a Slot.indication to
the VNF in every slot (in divergence from the nFAPI spec). 

Currently, downlink transmissions work the same in monolithic and nFAPI. In
uplink, we observe an increased number of retransmissions, which limits the MCS
and hence the achievable throughput (which is limited to 10-20Mbps). We are still
debugging the root cause of this.

After stopping the PNF, you also have to restart the VNF.

When using RFsim, the system might run slower than in monolithic. This is
because the PNF needs to slow down the execution time of a specific slot,
because it has to send a Slot.indication to the VNF for scheduling.

# Configuration

Both PNF and VNF are run through the `nr-softmodem` executable. The type of
mode is switched through the `--nfapi` switch, with options `MONOLITHIC`
(default if not provided), `VNF`, `PNF`.

If the type is `VNF`, you have to modify the `MACRLCs.tr_s_preference`
(transport south preference) to `nfapi`. Further, configure these options:

- `MACRLCs.remote_s_address` (remote south address): IP of the PNF
- `MACRLCs.local_s_address` (local south address): IP of the VNF
- `MACRLCs.local_s_portc` (local south port for control): VNF's P5 local port
- `MACRLCs.remote_s_portc` (remote south port for data): PNF's P5 remote port
- `MACRLCs.local_s_portd` (local south port for control): VNF's P5 local port
- `MACRLCs.remote_s_portd` (remote south port for data): PNF's P7 remote port

Note that any L1-specific section (`L1s`, `RUs`,
RFsimulator-specific/IF7.2-specific configuration or other radios, if
necessary) will be ignored and can be deleted.

If the type is `PNF`, you have to modify modify the `L1s.tr_n_preference`
(transport north preference) to `nfapi`. Further, configure these options:

- `L1s.remote_n_address` (remote north address): IP of the VNF
- `L1s.local_n_address` (local north address): IP of the PNF
- `L1s.local_n_portc` (local north port for control): PNF's P5 local port
- `L1s.remote_n_portc` (remote north port for control): VNF's P5 remote port
- `L1s.local_n_portd` (local north port for data): PNF's P7 local port
- `L1s.remote_n_portd` (remote north port for data): VNF's P7 remote port

Note that this file should contain additional, L1-specific sections (`L1s`,
`RUs` RFsimulator-specific/IF7.2-specific configuration or other radios, if
necessary).

To split an existing config file `monolithic.conf` for nFAPI operation, you
can proceed as follows:

- copy `monolithic.conf`, which will be your VNF file (`vnf.conf`)
- in `vnf.conf`
  * modify `MACRLCs` section to configure south-bound nFAPI transport
  * delete `L1s`, `RUs`, and radio-specific sections.
  * in `gNBs` section, select a sufficiently large `ra_ResponseWindow` if a UE
    does not connect with a message that the response window timed out:
    this is necessary because the PNF triggers the scheduler in the VNF
    in advance, which might make the RA window more likely to run out
- copy `monolithic.conf`, which will be your PNF file (`pnf.conf`)
- in `pnf.conf`
  * modify `L1s` section to configure north-bound nFAPI transport (make sure it
    matches the `MACRLCs` section for `vnf.conf`
  * delete all the `gNBs`, `MACRLCs`, `security` sections (they are not needed)
- if you have root-level options in `monolithic.conf`, such as
  `usrp-tx-thread-config` or `tune-offset`, make sure to to add them to
  `pnf.conf`, or provide them on the command line for the PNF.
- to run, proceed as described in the quick start above.

Note: all L1-specific options have to be passed to the PNF, and remaining
options to the VNF.

## Transport mechanisms between VNF and PNF

Currently, the VNF/PNF split supports three transport mechanisms between each
other:

1. Socket communication (either regular, or SCTP), this is the default

   The socket type may be changed by editing `nfapi_pnf_config_create()` and
   `nfapi_vnf_config_create()`, in both of which `_this->sctp = <value, 0 or
   1>;` indicate whether SCTP or regular sockets are to be used.  Note: The
   value of `_this->sctp` **must** be the same on the VNF and PNF.
2. Intel WLS Lib, which uses DPDK to achieve a shared memory communication between components.
3. nvIPC, which is used exclusively for the NVIDIA Aerial L1. Thus, it is only
   applicable for the VNF.

The change between transport mechanisms is done at compilation time:
- No changes to the `build_oai` call are required in order to select socket communication, as it is the default.
- In order to select WLS as the transport mechanism between VNF and PNF, first install the WLS library, and afterwards use `-t WLS` as a parameter of `build_oai`:

### How to use nFAPI

nFAPI is used by default. Compile and configure as indicated above.

### How to use Aerial

Refer to [this document](./Aerial_FAPI_Split_Tutorial.md) for more information.

### How to use WLS lib

Before the first compilation with WLS support, the [WLS
library](https://docs.o-ran-sc.org/projects/o-ran-sc-o-du-phy/en/latest/wls-lib.html)
must first be compiled and installed to the system.

The WLS library has a few dependencies:
  - [DPDK](https://doc.dpdk.org/guides/prog_guide/build-sdk-meson.html), specifically version [20.11.3](https://fast.dpdk.org/rel/dpdk-20.11.3.tar.xz).
  - libelf-dev
  - libhugetlbfs-dev

Additionally, a patch needs to be applied to the WLS lib Makefile in order for
the shared library and headers to be installed into the system, the necessary
patch is available [here](../cmake_targets/tools/install_wls_lib.patch)

Clone the code and apply the patch

    git clone -b oran_f_release https://gerrit.o-ran-sc.org/r/o-du/phy.git
    cd phy/wls_lib/
    git apply ~/openairinterface5g/cmake_targets/tools/install_wls_lib.patch

Then compile and install the library

    WIRELESS_SDK_TOOLCHAIN=gcc WIRELESS_SDK_TARGET_ISA=avx2 make
    sudo WIRELESS_SDK_TOOLCHAIN=gcc WIRELESS_SDK_TARGET_ISA=avx2 make install

After installing WLS, you can run the build command as shown below:

    ./build_oai -t WLS -w USRP --gNB --nrUE --ninja -C

#### How to run OAI PNF with OAI VNF

Refer to the above steps in [Quickstart](#quickstart), but run the PNF first as it is the WLS "master".

#### How to run OAI PNF with OSC/Radisys O-DU

Set up the hugepages for DPDK (1GB page size, 6 pages; this only needs to be
done once):

    sudo nano /etc/default/grub
          GRUB_CMDLINE_LINUX_DEFAULT="${GRUB_CMDLINE_LINUX_DEFAULT} default_hugepagesz=1G hugepagesz=1G hugepages=6"

Then rewrite the bootloader and reboot

    sudo update-grub
    sudo reboot 

Install dependencies:

    sudo apt-get install libstdc++-14-dev libnsl-dev libpcap-dev libxml2-dev

Clone the Radisys repository (We're currently using the `oai_integration` branch):

    git clone https://gerrit.o-ran-sc.org/r/o-du/l2 -b oai-integration

Build the O-DU

    cd ~/l2/build/odu
    make clean_all;make odu PHY=INTEL_L1 MACHINE=BIT64 MODE=TDD;make cu_stub NODE=TEST_STUB MACHINE=BIT64 MODE=TDD;make ric_stub NODE=TEST_STUB MACHINE=BIT64 MODE=TDD

Setup local interfaces for the cu_stub, ric_stub and o_du

    sudo ifconfig enp7s0:ODU "192.168.130.81" && sudo ifconfig enp7s0:CU_STUB "192.168.130.82" && sudo ifconfig enp7s0:RIC_STUB "192.168.130.80"

Run cu_stu and ric_stub in separate terminals

    cd ~/l2/bin/
    ./cu_stub/cu_stub 
    clear && ./ric_stub/ric_stub

Run the OAI PNF first, as it is the WLS memory master

    sudo NFAPI_TRACE_LEVEL=info ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb-pnf.band78.rfsim.2x2.conf --nfapi PNF --rfsim --rfsimulator.serveraddr server

Run the O-DU over GDB

    sudo -E gdb -ex run --readnever --args  ./odu/odu

Note: If you see the following prompt in GDB

    This GDB supports auto-downloading debuginfo from the following URLs:
      <https://debuginfod.ubuntu.com>
    Enable debuginfod for this session? (y or [n]) 

You can run the following command one time before executing gdb to disable it: 

    export DEBUGINFOD_URLS=

Run the OAI-UE

    sudo ./nr-uesoftmodem -r 273 --numerology 1 --band 78 -C 3400140000 --ssb 1518 --uicc0.imsi 001010000000001 --rfsim

# nFAPI logging system

nFAPI has its own logging system, independent of OAI's. It can be activated by
setting the `NFAPI_TRACE_LEVEL` environment variable to an appropriate value;
see [the environment variables documentation](./environment-variables.md) for
more info.

To see the (any) periodical output at the PNF, define `NFAPI_TRACE_LEVEL=info`.
This output shows:

```
41056.739654 [I] 3556767424: pnf_p7_slot_ind: [P7:1] msgs ontime 489 thr DL 0.06 UL 0.01 msg late 0 (vtime)
```

The first numbers are timestamps. `pnf_p7_slot_ind` is the name of the
functions that prints the output. `[P7:1]` refers to the fact that these are
information on P7, of PHY ID 1. Finally, `msgs ontime 489` means that in the
last window (since the last print), 489 messages arrived at the PNF in total.
The combined throughput of `TX_data.requests` (DL traffic) was 0.06 Mbps; note
that this includes SIB1 and other periodical data channels. In UL, 0.01 Mbps
have been sent through `RX_data.indication`. `msg late 0` means that 0 packets
have been late. _This number is an aggregate over the total runtime_, unlike
the other messages. Finally, `(vtime)` is a reminder that the calculations are
done over virtual time, i.e., frames/slots as executed by the 5G Systems. For
instance, these numbers might be slightly higher or slower in RFsim than in
wall-clock time, depending if the system advances faster or slower than
wall-clock time.

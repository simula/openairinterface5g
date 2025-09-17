<table style="border-collapse: collapse; border: none;">
  <tr style="border-collapse: collapse; border: none;">
    <td style="border-collapse: collapse; border: none;">
      <a href="http://www.openairinterface.org/">
         <img src="./images/oai_final_logo.png" alt="" border=3 height=50 width=150>
         </img>
      </a>
    </td>
    <td style="border-collapse: collapse; border: none; vertical-align: center;">
      <b><font size = "5">OAI LDPC offload (O-RAN AAL/DPDK BBDEV)</font></b>
    </td>
  </tr>
</table>

**Table of Contents**

[[_TOC_]]

This documentation describes the integration of LDPC coding for lookaside acceleration using O-RAN AAL/DPDK BBDEV in OAI, along with its usage.
For details on the implementation, please consult the [developer notes](../openair1/PHY/CODING/nrLDPC_coding/nrLDPC_coding_aal/README.md).

# Requirements

In principle, any lookaside LDPC accelerator supporting the O-RAN AAL/DPDK BBDEV should work.
However, the current implementation has only been validated for the Xilinx T2, Intel ACC100, and Intel ACC200 (VRB1).
Therefore, your mileage may vary when using other BBDEV devices as there may be some hardware-specific changes required -- contributions are welcome!

## DPDK Version Requirements

The following DPDK versions are supported:
- For the Xilinx T2 card, DPDK20.11+ is supported.
- As for the Intel ACC100/ACC200, only DPDK22.11+ is supported.

## Tested Devices/ DPDK versions

### Xilinx T2

- DPDK20.11.9*.
- DPDK22.11.7*.
> Note: FPGA bitstream image and the corresponding patch file (e.g., `ACCL_BBDEV_DPDK20.11.3_ldpc_3.1.918.patch` for DPDK20.11) from Accelercomm required.

### Intel ACC100

- DPDK22.11.7*.
- DPDK23.11.3*.
- DPDK24.11.2.
> Note: [Patch]((https://github.com/DPDK/dpdk/commit/fdde63a1dfc129d0a510a831aa98253b36a2a1cd)) required for pre-DPDK24.11 versions when using the Intel ACC100.

### Intel ACC200 (also known as VRB1)
- DPDK22.11.7.
- DPDK23.11.3.
- DPDK24.11.2.

# System Setup
## DPDK installation

> Important: 
> - If you are using the Xilinx T2 card, you will need to apply the vendor-supplied patches before compiling DPDK. 
> - If you are using the Intel ACC100, you will need to [patch](https://github.com/DPDK/dpdk/commit/fdde63a1dfc129d0a510a831aa98253b36a2a1cd) the ACC100's driver if you are using DPDK22.11 or DPDK23.11. 


Refer to the guide [here](./ORAN_FHI7.2_Tutorial.md?ref_type=heads#dpdk-data-plane-development-kit) to install, and then validate your DPDK installation.

<details open> 
<summary> Notes on DPDK patching/installation for Xilinx T2. </summary>

*Note: The following instructions apply to `ACCL_BBDEV_DPDK20.11.3_ldpc_3.1.918.patch`, compatible with DPDK 20.11.9. For older patches (e.g., `ACL_BBDEV_DPDK20.11.3_BL_1006_build_1105_dev_branch_MCT_optimisations_1106_physical_std.patch`), refer to the T2 documentation in `2023.w48`.*

```bash
# Get DPDK source code
git clone https://github.com/DPDK/dpdk-stable.git ~/dpdk-stable
cd ~/dpdk-stable
git checkout v20.11.9
git apply ~/ACL_BBDEV_DPDK20.11.3_ldpc_3.1.918.patch
```
Replace `~/ACL_BBDEV_DPDK20.11.3_ldpc_3.1.918.patch` by patch file provided by
Accelercomm.

If you would like to install DPDK to a custom directory, here is an example.
```bash
cd ~/dpdk-stable
# meson setup build
meson setup --prefix=/opt/dpdk-t2 build # for installation with non-default installation prefix
cd build
ninja
sudo ninja install
sudo ldconfig

```
</details>

## System configuration

### Setting up Hugepages

First, we must setup hugepages on the system.
In our setup, we setup 16 of the 1G hugepages.
Apart from 1G, 2MB hugepages works too, but make sure to allocate a sufficient number of them.

```
# sudo dpdk-hugepages.py -p 1G --setup 16G
```

### Locating the Accelerator

Next, we check whether our system can detect our accelerator using `dpdk-devbind.py`.
You should see Baseband devices detected by DPDK, as follows:
```
# sudo dpdk-devbind.py -s
...
Baseband devices using DPDK-compatible driver
=============================================
0000:f7:00.0 'Device 57c0' unused=vfio-pci
...
```

As you can see here, our Intel ACC200 has the address of `0000:f7:00.0`.
Depending on the accelerator you are using, the address may vary.

### Loading VFIO-PCI and enabling SR-IOV
Following, make sure to load the `vfio-pci` kernel modules and ensure that SR-IOV is enabled.

```
# sudo modprobe vfio-pci enable_sriov=1 disable_idle_d3=1
```

### Binding the Accelerator with `vfio-pci`

Lastly, we bind our accelerator with the `vfio-pci` driver.
```
# sudo dpdk-devbind.py --bind=vfio-pci 0000:f7:00.0
```

> Note: For the Xilinx T2, we can use this device directly.
If you use an Intel vRAN accelerator, read on.

### Additional Steps for Intel vRAN Accelerators

> IMPORTANT NOTE: 
> - Currently, we only support using the Virtual Functions (VFs) of the Intel vRAN accelerators, but not the Physical Function (PF). 
> - One key advantage of using VFs is that this allows us to share the accelerator with other DU instances on the same machine, which is common in practice.

If you are using an Intel vRAN accelerator, you will need to use the [pf_bb_config](https://github.com/intel/pf-bb-config) tool to configure the accelerator beforehand. 

#### pf_bb_config
For more details, please consult the `pf_bb_config` README.

```
# git clone https://github.com/intel/pf-bb-config
# cd ~/pf-bb-config
# ./build.sh
```
This clones and builds the `pf_bb_config` binary.

Next, we show an example for the Intel ACC200.
We use an existing configuration located at `./vrb1/vrb1_config_16vf.cfg`.

Here, it is necessary to specify a VFIO token (in this case, we use the UUID `00112233-4455-6677-8899-aabbccddeeff`).
Note that in practice, a random UUID should be used.
```
# sudo ./pf_bb_config VRB1 -v 00112233-4455-6677-8899-aabbccddeeff -c vrb1/vrb1_config_16vf.cfg
== pf_bb_config Version v25.01-0-g812e032 ==
VRB1 PF [0000:f7:00.0] configuration complete!
Log file = /var/log/pf_bb_cfg_0000:f7:00.0.log
```

#### Creating VFs

Finally, we create the VF(s) for our accelerator. 
In this example, we only create one SR-IOV VF.
```
# echo 1 | sudo tee /sys/bus/pci/devices/0000:f7:00.0/sriov_numvfs
```

If you encounter any error when creating the VF(s), e.g., `tee: '/sys/bus/pci/devices/0000:f7:00.0/sriov_numvfs': No such file or directory`, then try enabling SR-IOV again.
```
# echo 1 | sudo tee /sys/module/vfio_pci/parameters/enable_sriov
```

After you have successfully created the VF, you should see an additional baseband device, in our case, it is `0000:f7:00.1`. 
We will use this device with OAI later.
The newly created VF should also be using the same `vfio-pci` driver as the PF, if it is not, you will need to do a `dpdk-devbind.py` to bind it with `vfio-pci`. 
```
# sudo dpdk-devbind.py -s
...
Baseband devices using DPDK-compatible driver
=============================================
0000:f7:00.0 'Device 57c0' drv=vfio-pci unused=
0000:f7:00.1 'Device 57c0' drv=vfio-pci unused=
...
```

# Building OAI with ORAN-AAL
OTA deployment is precisely described in the following tutorial:
- [NR_SA_Tutorial_COTS_UE](https://gitlab.eurecom.fr/oai/openairinterface5g/-/blob/develop/doc/NR_SA_Tutorial_COTS_UE.md)
Instead of section *3.2 Build OAI gNB* from the tutorial, run the following commands:

```bash
# Get openairinterface5g source code
git clone https://gitlab.eurecom.fr/oai/openairinterface5g.git ~/openairinterface5g
cd ~/openairinterface5g
git checkout develop

# Install OAI dependencies
cd ~/openairinterface5g/cmake_targets
./build_oai -I

# Build OAI gNB
cd ~/openairinterface5g
source oaienv
cd cmake_targets
./build_oai -w USRP --ninja --gNB -P --build-lib "ldpc_aal" -C
```

A shared object file `libldpc_aal.so` will be created during the compilation.
This object is conditionally compiled. 
The selection of the library to compile is done using `--build-lib ldpc_aal`.

> Note: The required DPDK poll mode driver has to be present on the host machine and required DPDK version has to be installed on the host, prior to building OAI.

# O-RAN AAL DPDK EAL parameters
To configure O-RAN AAL/DPDK BBDEV, you can set the following parameters via the command line of PHY simulators or nr-softmodem:

> Note: the group parameter name has been renamed from `nrLDPC_coding_t2` to
> `nrLDPC_coding_aal` to better reflect that it is a generic AAL accelerator
> card.

- `nrLDPC_coding_aal.dpdk_dev` - **mandatory** parameter, specifies the PCI address of our accelerator. It must follow the format `WWWW:XX:YY.Z`.

- `nrLDPC_coding_aal.dpdk_core_list` - **mandatory** parameter, specifies the CPU cores assigned to DPDK . 
Ensure that the CPU cores specified in `nrLDPC_coding_aal.dpdk_core_list` are available and not used by other processes to avoid conflicts.

- `nrLDPC_coding_aal.dpdk_prefix` - optional parameter, DPDK shared data file prefix, by default set to *b6*.

- `nrLDPC_coding_aal.vfio_vf_token` - optional parameter, VFIO token set for the VF, if applicable.

- `nrLDPC_coding_aal.num_harq_codeblock` - optional parameter, size of the HARQ buffer in terms of the number of 32kB blocks, by default set to *512* (maximum for the T2; as for the ACCs, this can be further increased).

- `nrLDPC_coding_aal.is_t2` - optional parameter, set this to 1 when using the Xilinx T2 card.

**Note:** These parameters can also be provided in a configuration file.
Example for the ACC200:
```
nrLDPC_coding_aal : {
  dpdk_dev : "0000:f7:00.1";
  dpdk_core_list : "14-15";
  vfio_vf_token: "00112233-4455-6677-8899-aabbccddeeff";
};

loader : {
  ldpc : {
    shlibversion : "_aal";
  };
};
```

# Running OAI with O-RAN AAL

In general, to offload of the channel coding to the LDPC accelerator, we use use the `--loader.ldpc.shlibversion _aal` option.
Reminder, if you are using the Xilinx T2 card, make sure to set `--nrLDPC_coding_aal.is_t2 1`.

## 5G PHY simulators

### nr_ulsim

Example command:
```bash
cd ~/openairinterface5g
source oaienv
cd cmake_targets/ran_build/build
sudo ./nr_ulsim -n100 -s20 -m20 -r273 -R273 --loader.ldpc.shlibversion _aal --nrLDPC_coding_aal.dpdk_dev 0000:f7:00.1 --nrLDPC_coding_aal.dpdk_core_list 0-1 --nrLDPC_coding_aal.vfio_vf_token 00112233-4455-6677-8899-aabbccddeeff
```
### nr_dlsim

Example command:
```bash
cd ~/openairinterface5g
source oaienv
cd cmake_targets/ran_build/build
sudo ./nr_dlsim -n300 -s30 -R 106 -e 27 --loader.ldpc.shlibversion _aal --nrLDPC_coding_aal.dpdk_dev 0000:f7:00.1 --nrLDPC_coding_aal.dpdk_core_list 0-1 --nrLDPC_coding_aal.vfio_vf_token 00112233-4455-6677-8899-aabbccddeeff
```

## OTA test

### Running OAI gNB with USRP B210/FHI72

When running the gNB **with FHI 7.2**, it is not necessary to provide the `--nrLDPC_coding_aal.dpdk_core_list` argument
since the core list specified for FHI 7.2 will be used for DPDK.
If it is provided, the AAL core list wil be ignored.  

Example command:
```bash
cd ~/openairinterface5g
source oaienv
cd cmake_targets/ran_build/build
sudo ./nr-softmodem -O ~/gnb.conf --loader.ldpc.shlibversion _aal --nrLDPC_coding_aal.dpdk_dev 0000:f7:00.1 --nrLDPC_coding_aal.dpdk_core_list 14-15 --nrLDPC_coding_aal.vfio_vf_token 00112233-4455-6677-8899-aabbccddeeff
```

# Known Issue(s)

## Potential Low Throughput

The current implementation has been tested to work in an end-to-end setup and is functional. 
However, depending on the accelerator in use,
there are still opportunities for optimization, particularly in LDPC decoding performance, which is an area of ongoing improvement. 
As such, downlink/uplink throughput may be suboptimal with the default configurations, but enhancements are actively being explored.

For example, to achieve better E2E performance with the current implementation with Intel ACC 100 and 200 (vRAN Boost),
we recommend the following adjustments:
1. Increasing the number of LDPC decoding iterations of the L1, e.g., `max_ldpc_iterations` to 200.
2. Increasing the BLER targets of the MAC scheduler. 

Example configuration snippet:
```
...
MACRLCs = (
{
  num_cc                      = 1;
  tr_s_preference             = "local_L1";
  tr_n_preference             = "local_RRC";
  pusch_TargetSNRx10          = 180;
  pucch_TargetSNRx10          = 220;
  dl_bler_target_upper        = .35;
  dl_bler_target_lower        = .15;
  ul_bler_target_upper        = .35;
  ul_bler_target_lower        = .15;
  pusch_FailureThres          = 1000;
  ul_max_mcs                  = 28;

}
);

L1s = (
{
  num_cc = 1;
  tr_n_preference       = "local_mac";
  prach_dtx_threshold   = 120;
  pucch0_dtx_threshold  = 100;
  ofdm_offset_divisor   = 8; #set this to UINT_MAX for offset 0
  max_ldpc_iterations = 200;
}
);
...
```

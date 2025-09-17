<table style="border-collapse: collapse; border: none;">
  <tr style="border-collapse: collapse; border: none;">
    <td style="border-collapse: collapse; border: none;">
      <a href="http://www.openairinterface.org/">
         <img src="./images/oai_final_logo.png" alt="" border=3 height=50 width=150>
         </img>
      </a>
    </td>
    <td style="border-collapse: collapse; border: none; vertical-align: center;">
      <b><font size = "5">OAI 7.2 Fronthaul Interface 5G SA Tutorial</font></b>
    </td>
  </tr>
</table>

**Table of Contents**

[[_TOC_]]

# Prerequisites

The hardware on which we have tried this tutorial:

| Hardware (CPU,RAM)                                                         |Operating System (kernel)                  | NIC (Vendor,Driver,Firmware)                     |
|----------------------------------------------------------------------------|----------------------------------|--------------------------------------------------|
| Gigabyte  Edge E251-U70 (Intel Xeon Gold 6240R, 2.4GHz, 24C48T, 96GB DDR4) |Ubuntu 22.04.3 LTS (5.15.0-72-lowlatency)| NVIDIA ConnectX®-6 Dx 22.38.1002                 |
| Dell PowerEdge R750 (Dual Intel Xeon Gold 6336Y CPU @ 2.4G, 24C/48T (185W), 512GB RDIMM, 3200MT/s) |Ubuntu 22.04.3 LTS (5.15.0-72-lowlatency)| NVIDIA Converged Accelerator A100X  (24.39.2048) |
| Supermicro Grace Hopper MGX ARS-111GL-NHR (Neoverse-V2, 3.4GHz, 72C/72T, 576GB LPDDR5) | Ubuntu 22.04.5 LTS (6.5.0-1019-nvidia-64k) |NVIDIA BlueField3 (32.41.1000)|

**NOTE**:
- These are not minimum hardware requirements. This is the configuration of our servers. The NIC card should support hardware PTP time stamping.
- Starting from tag [2025.w13](https://gitlab.eurecom.fr/oai/openairinterface5g/-/tree/2025.w13?ref_type=tags) of OAI, we are only testing with the Grace Hopper server.

PTP enabled switches and grandmaster clock we have tested with:

| Vendor                   | Software Version |
|--------------------------|------------------|
| Fibrolan Falcon-RX/812/G | 8.0.25.4         |
| CISCO C93180YC-FX3       | 10.2(4)          |
| Qulsar Qg2 (Grandmaster) | 12.1.27          |


These are the radio units we've used for testing:

| Vendor                | Software Version            |
|-----------------------|-----------------------------|
| Foxconn RPQN-7801E RU | 2.6.9r254                   |
| Foxconn RPQN-7801E RU | 3.1.15_0p4                  |
| Foxconn RPQN-7801E RU | 3.2.0q.551.12.E.rc2.srs-AIO |
| WNC     R1220-078LE   | 1.9.0                       |

The UEs that have been tested and confirmed working with Aerial are the following:

| Vendor          | Model                         |
|-----------------|-------------------------------|
| Sierra Wireless | EM9191                        |
| Quectel         | RM500Q-GL                     |
| Quectel         | RM520N-GL                     |
| Apaltec         | Tributo 5G-Dongle             |
| OnePlus         | Nord (AC2003)                 |
| Apple iPhone    | 14 Pro (MQ0G3RX/A) (iOS 17.3) |
| Samsung         | S23 Ultra                     |


## Configure your server

To set up the L1 and install the components manually refer to this [instructions page](https://docs.nvidia.com/aerial/cuda-accelerated-ran/index.html).

**Note**:
- As of wk36, the L1 must be compiled with the following CMake flag: `-DSCF_FAPI_10_04_SRS=ON` , this is due to the usage
of the FAPI 10.04 version of the SRS PDU, and RX_Beamforming PDU.
- To configure the Gigabyte server please refer to these [instructions](https://gitlab.eurecom.fr/oai/openairinterface5g/-/blob/2025.w13/doc/Aerial_FAPI_Split_Tutorial.md)
- The last release to support the Gigabyte server is **Aerial CUDA-Accelerated RAN 24-1**.

### CPU allocation

| Server brand     | Model         | Nº of CPU Cores | Isolated CPUs  |
|------------------|---------------|:---------------:|:--------------:|
| Grace Hopper MGX | ARS-111GL-NHR |      72         |      4-64      |

**Grace Hopper MGX ARS-111GL-NHR**

| Applicative Threads    | Allocated CPUs |
|------------------------|----------------|
| PTP & PHC2SYS Services | 41             |
| OAI `nr-softmodem`     | 13-14          |

## PTP configuration

1. You need to install the `linuxptp` debian package. It will install both ptp4l and phc2sys.

```bash
#Ubuntu
sudo apt install linuxptp -y
```

Once installed you can use this configuration file for ptp4l (`/etc/ptp4l.conf`). Here the clock domain is 24 so you can adjust it according to your PTP GM clock domain

```
[global]
domainNumber            24
slaveOnly               1
time_stamping           hardware
tx_timestamp_timeout    30

[your_PTP_ENABLED_NIC]
network_transport       L2

```

The service of ptp4l (`/lib/systemd/system/ptp4l.service`) should be configured as below:
```
[Unit]
Description=Precision Time Protocol (PTP) service
Documentation=man:ptp4l
 
[Service]
Restart=always
RestartSec=5s
Type=simple
ExecStart=taskset -c 41 /usr/sbin/ptp4l -f /etc/ptp.conf
 
[Install]
WantedBy=multi-user.target

```

and service of phc2sys (`/lib/systemd/system/phc2sys.service`) should be configured as below:
```
[Unit]
Description=Synchronize system clock or PTP hardware clock (PHC)
Documentation=man:phc2sys
After=ntpdate.service
Requires=ptp4l.service
After=ptp4l.service

[Service]
Restart=always
RestartSec=5s
Type=simple
ExecStart=/usr/sbin/phc2sys -a -r -n 24

[Install]
WantedBy=multi-user.target

```

# Build OAI gNB

If it's not already cloned, the first step is to clone OAI repository
```bash
git clone https://gitlab.eurecom.fr/oai/openairinterface5g.git ~/openairinterface5g
cd ~/openairinterface5g/
```

## Get nvIPC sources from the L1 container 

The library used for communication between L1 and L2 components is called nvIPC, and is developed by NVIDIA. It is not open-source and can't be freely distributed.
In order to achieve this communication, we need to obtain the nvIPC source files from the L1 container (cuBB) and place it in the gNB project directory `~/openairinterface5g`.
This allows us to build and install this library when building the L2 docker container.

Check whether your L1 container is running:
```bash
~$ docker container ls -a
CONTAINER ID        IMAGE               COMMAND                  CREATED             STATUS                      PORTS               NAMES
a9681a0c4a10        14dca2002237        "/opt/nvidia/nvidia_…"   3 months ago        Exited (137) 10 days ago                        cuBB

```
If it is not running, you may start it and logging into the container by running the following:
```bash
~$ docker start cuBB
cuBB
~$ docker exec -it cuBB bash
aerial@c_aerial_aerial:/opt/nvidia/cuBB# 
```

After logging into the container, you need to pack the nvIPC sources and copy them to the host ( the command creates a `tar.gz` file with the following name format: `nvipc_src.YYYY.MM.DD.tar.gz`)
```bash
~$ docker exec -it cuBB bash
aerial@c_aerial_aerial:/opt/nvidia/cuBB# cd cuPHY-CP/gt_common_libs
aerial@c_aerial_aerial:/opt/nvidia/cuBB/cuPHY-CP/gt_common_libs#./pack_nvipc.sh 
nvipc_src.YYYY.MM.DD/
...
---------------------------------------------
Pack nvipc source code finished:
/opt/nvidia/cuBB/cuPHY-CP/gt_common_libs/nvipc_src.YYYY.MM.DD.tar.gz
aerial@c_aerial_aerial:/opt/nvidia/cuBB/cuPHY-CP/gt_common_libs# sudo cp nvipc_src.yyyy.mm.dd.tar.gz /opt/cuBB/share/
aerial@c_aerial_aerial:/opt/nvidia/cuBB/cuPHY-CP/gt_common_libs# exit
```

The file should now be present in the `~/openairinterface5g/ci-scripts/yaml_files/sa_gnb_aerial/` directory, from where it is moved into `~/openairinterface5g`
```bash
~$ mv ~/openairinterface5g/ci-scripts/yaml_files/sa_gnb_aerial/nvipc_src.YYYY.MM.DD.tar.gz ~/openairinterface5g/
```

Alternatively, after running `./pack_nvipc.sh` , you can exit the container and copy `nvipc_src.YYYY.MM.DD.tar.gz` using `docker cp`

```bash
~$ docker exec -it cuBB bash
aerial@c_aerial_aerial:/opt/nvidia/cuBB# cd cuPHY-CP/gt_common_libs
aerial@c_aerial_aerial:/opt/nvidia/cuBB/cuPHY-CP/gt_common_libs#./pack_nvipc.sh 
nvipc_src.YYYY.MM.DD/
...
---------------------------------------------
Pack nvipc source code finished:
/opt/nvidia/cuBB/cuPHY-CP/gt_common_libs/nvipc_src.YYYY.MM.DD.tar.gz
aerial@c_aerial_aerial:/opt/nvidia/cuBB/cuPHY-CP/gt_common_libs# exit
~$ docker cp nv-cubb:/opt/nvidia/cuBB/cuPHY-CP/gt_common_libs/nvipc_src.YYYY.MM.DD.tar.gz ~/openairinterface5g
```
*Important note:* For using docker cp, make sure to copy the entire name of the created nvipc_src tar.gz file. 

With the nvIPC sources in the project directory, the docker image can be built.

## Building OAI gNB docker image

In order to build the target image (`oai-gnb-aerial`), first you should build a common shared image (`ran-base`)
```bash
~$ cd ~/openairinterface5g/
~/openairinterface5g$ docker build . -f docker/Dockerfile.base.ubuntu --tag ran-base:latest
~/openairinterface5g$ docker build . -f docker/Dockerfile.gNB.aerial.ubuntu --tag oai-gnb-aerial:latest
```


# Running the setup

In order to use Docker Compose to automatically start and stop the setup, we first need to create a pre-built image of L1 after compiling the L1 software and making the necessary adjustments to its configuration files.
The process of preparing L1 is covered in NVIDIA's documentation and falls outside the scope of this document.

## Prepare the L1 image
After preparing the L1 software, the container needs to be committed to create an image where L1 is ready for execution, which can later be referenced by a `docker-compose.yaml` file.

*Note:* The default L1 configuration file is `cuphycontroller_P5G_FXN_GH.yaml`, located in `/opt/nvidia/cuBB/cuPHY-CP/cuphycontroller/config/`.
In this file the RU MAC address needs to be specified before commiting the image.

```bash
~$ docker commit nv-cubb cubb-build:25-2
~$ docker image ls
..
cubb-build                                    25-2                                           824156e0334c   2 weeks ago    23.9GB
-..
```

## Adapt the OAI-gNB configuration file to your system/workspace

Edit the sample OAI gNB configuration file and check following parameters:

* `gNBs` section
  * The PLMN section shall match the one defined in the AMF
  * `amf_ip_address` shall be the correct AMF IP address in your system
  * `GNB_IPV4_ADDRESS_FOR_NG_AMF` shall match your DU N2 interface IP address
  * `GNB_IPV4_ADDRESS_FOR_NGU` shall match your DU N3 interface IP address
  
The default amf_ip_address:ipv4 value is 192.168.70.132, when installing the CN5G following [this tutorial](https://gitlab.eurecom.fr/oai/openairinterface5g/-/blob/develop/doc/NR_SA_Tutorial_OAI_CN5G.md)
Both 'GNB_IPV4_ADDRESS_FOR_NG_AMF' and 'GNB_IPV4_ADDRESS_FOR_NGU' need to be set to the IP address of the NIC referenced previously.


## Running docker compose
### Aerial L1 entrypoint script
The `aerial_l1_entrypoint` script is used by the L1 container to start the L1 software and is called in the Docker Compose file.
It begins by setting up environment variables, restarting NVIDIA MPS, and finally running `cuphycontroller_scf`.

The L1 software is executed with an argument that specifies which configuration file to use. If not modified, the default argument is set to `P5G_FXN_GH`.

After building the gNB image, and preparing the configuration file, the setup can be run with the following command:

```bash
cd ci-scripts/yaml_files/sa_gnb_aerial/
docker compose up -d
 
```
This will start both containers, beginning with `nv-cubb`, and `oai-gnb-aerial` will start only after it is ready.

The logs can be followed using these commands:

```bash
docker logs -f oai-gnb-aerial
docker logs -f nv-cubb
```
### Running with multiple L2s
One L1 instance can support multiple L2 instances. See also the [aerial documentation](https://developer.nvidia.com/docs/gputelecom/aerial-sdk/text/cubb_quickstart/running_cubb-end-to-end.html#run-multiple-l2-instances-with-single-l1-instance) for more details.

In OAI the shared memory prefix must be configured in the configuration file.

```bash
        tr_s_preference = "aerial";
	tr_s_shm_prefix = "nvipc";
```


## Stopping the setup

Run the following command to stop and remove both containers, leaving the system ready to be restarted later:
```bash
cd ci-scripts/yaml_files/sa_gnb_aerial/
docker compose down
 
```

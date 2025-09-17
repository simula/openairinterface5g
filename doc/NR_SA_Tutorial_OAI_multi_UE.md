<table style="border-collapse: collapse; border: none;">
  <tr style="border-collapse: collapse; border: none;">
    <td style="border-collapse: collapse; border: none;">
      <a href="http://www.openairinterface.org/">
         <img src="./images/oai_final_logo.png" alt="" border=3 height=50 width=150>
         </img>
      </a>
    </td>
    <td style="border-collapse: collapse; border: none; vertical-align: center;">
      <b><font size = "5">OAI 5G NR SA tutorial to deploy multiple OAI nrUE</font></b>
    </td>
  </tr>
</table>

**Table of Contents**

[[_TOC_]]

# Scenario
This tutorial is about how to configure and run multiple OAI nrUE in the same end-to-end OAI 5G setup with RFsimulator.

# Pre-requisites

This tutorial is assuming that OAI CN5G and OAI RAN are already deployed. To learn how to deploy and run a basic setup with OAI nrUE, please refer to [NR_SA_Tutorial_OAI_nrUE.md](NR_SA_Tutorial_OAI_nrUE.md).

Also, it is suggested to get some knowledge on how the channel simulation with OAI RFsimulator works. Please refer to the following documentation to learn about the relevant topics discussed:

- RFsimulator tutorial [rfsimulator/README.md](../radio/rfsimulator/README.md)
- Channel simulation with OAI [channel_simulation.md](../openair1/SIMULATION/TOOLS/DOC/channel_simulation.md)
- Telnet server usage [telnetusage.md](../common/utils/telnetsrv/DOC/telnetusage.md).

# Run multiple UEs in RFsimulator

## Multiple nrUEs with namespaces

Important notes:

* This should be run on the same host as the OAI gNB
* Use the script [multi_ue.sh](../tools/scripts/multi-ue.sh) to make namespaces for multiple UEs.
* For each UE, a namespace shall be created, each one has a different address that will be used as rfsim server address
* Each UE shall have a different IMSI, which shall be present in the relevant tables of the MySQL database
* Each UE shall run a telnet server on a different port, with command line option `--telnetsrv.listenport`

1. For the first UE, create the namespace ue1 (`-c1`), then execute shell inside (`-o1`, "open"):

   ```bash
   sudo ./multi-ue.sh -c1
   sudo ./multi-ue.sh -o1
   ```

2. After entering the bash environment, run the following command to deploy your first UE

   ```bash
   sudo ./nr-uesoftmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/ue.conf -r 106 --numerology 1 --band 78 -C 3619200000    --rfsim --uicc0.imsi 001010000000001 --rfsimulator.options chanmod --rfsimulator.serveraddr 10.201.1.100 --telnetsrv    --telnetsrv.listenport 9095
   ```

3. For the second UE, create the namespace ue2 (`-c2`), then execute shell inside (`-o2`, "open"):

   ```bash
   sudo ./multi-ue.sh -c2
   sudo ./multi-ue.sh -o2
   ```

4. After entering the bash environment, run the following command to deploy your second UE:

   ```bash
   sudo ./nr-uesoftmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/ue.conf -r 106 --numerology 1 --band 78 -C 3619200000    --rfsim --uicc0.imsi 001010000000002 --rfsimulator.options chanmod --rfsimulator.serveraddr 10.202.1.100 --telnetsrv    --telnetsrv.listenport 9096
   ```

in the command above, please note that the IMSI and the telnet port changed.

## Running Multiple UEs with Docker

1. Make sure OAI nrUE image is pulled:

   ```bash
   docker pull oaisoftwarealliance/oai-nr-ue:latest
   ```

2. Configure your setup by editing the Docker compose file e.g. in [docker-compose.yaml](../ci-scripts/yaml_files/5g_rfsimulator/docker-compose.yaml).

3. Deploy the UEs, e.g. for 3 UEs:

   ```bash
   docker compose up -d oai-nr-ue{1,2,3}
   ```

4. Check the logs to ensure each UE has gotten an IP address, e.g.:

   ```bash
   docker logs oai-nr-ue1
   ```

   or

   ```bash
   docker exec -it oai-nr-ue1 ip a show oaitun_ue1
   ```

5. Test the connectivity of each UE to the core network:

   ```bash
   docker exec -it oai-nr-ue1 ping -c1 192.168.70.135
   ```

7. After testing, undeploy the UEs to allow them time to deregister, and then bring down the rest of the network:

   ```bash
   docker compose stop oai-nr-ue{1,2,3}
   docker compose down -v
   ```

# Further reading

For more details and scenarios, refer to the following files:

* [RFSIM deployment in the CI](../ci-scripts/yaml_files/5g_rfsimulator/README.md)
* [E1 deployment in the CI](../ci-scripts/yaml_files/5g_rfsimulator_e1/README.md)
* [Docker documentation](../docker/README.md)

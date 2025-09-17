# Procedure to add dedicated Bandwidth part (BWP)

## Contributed by 5G Testbed IISc 

### Developers: Abhijith A, Shruthi S

# Terminology #

## Bandwidth part (BWP) ##
Bandwidth Part (BWP) is a set of contiguous Resource Blocks in the resource grid. 

Parameters of a BWP are communicated to UE using RRC parameters: BWP-Downlink and BWP-Uplink. 

A UE can be configured with a set of 4 BWPs in uplink (UL) and downlink (DL) direction (3GPP TS 38.331 Annex B.2 Description of BWP configuration options). But only 1 BWP can be active in UL and DL direction at a given time.

# Procedure to run multiple dedicated BWPs #

A maximum of 4 dedicated BWPs can be configured for a UE.

To configure multiple BWPs, add the following parameters in the physical parameters section:

## Setup of the Configuration files ##

In the configuration file you have the option to select the 1st active BWP, the RIV and SCS of each BWP in the following way (example with 3 additional BWPs):
```
    first_active_bwp = 1;
    bwp_list = ({ scs = 1; bwpStart = 0; bwpSize = 106;},
                { scs = 1; bwpStart = 0; bwpSize = 51;},
                { scs = 1; bwpStart = 0; bwpSize = 24;});
```

This example configures 3 additional BWPs, with IDs from 1 to 3. Find these parameters in this configuration file: "ci-scripts/conf_files/gnb.sa.band78.106prb.rfsim.neighbour.conf"
     
# Testing gNB and UE in RF simulator

## gNB command:
```
    sudo ./nr-softmodem -O ../../../ci-scripts/conf_files/gnb.sa.band78.106prb.rfsim.neighbour.conf --rfsim --rfsimulator.serveraddr server
```

## UE command:
```
    sudo ./nr-uesoftmodem -C 3319680000 -r 106 --numerology 1 --ssb 516 --band 78  --rfsim --rfsimulator.serveraddr 127.0.0.1
```

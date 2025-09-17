This document explains the implementation of analog beamforming in OAI codebase.

[[_TOC_]]

# Introduction to analog beamforming

Beamforming is a technique applied to antenna arrays to create a directional radiation pattern. This often consists in providing a different phase shift to each element of the array such that signals with a different angle of arrival/departure experience a change in radiation pattern because of constructive or destructive interference.

There are three main beamforming techinques: analog, digital and hybrid. The names refer to the phase shift application before or after the digital to analog conversion (or analog to digital in reception). When we speak about analog beamforming we generally refer to a techinique where the phase shifts that produce the beam stearing are applied by the radio unit (RU) choosing from a finite set of steering directions. The advantage of analog beamforming is a simplified analog circuitry and therefore reduced costs.

The presence of a limited number of predefined beams at RU poses constraints to the scheduler at gNB. As a matter of fact, the scheduler can serve only a limited number of beams, depending on the RU characteristics (possibly only 1), in a given time scale, that also depends on the RU characteristics (e.g. 1 slot or 1 symbol). This limitation doesn't exist for digital beamforming.

Analog beamforming implementation also allows to enable distributed antenna systems (DAS), where each beam corrisponds to one antenna (or a set of antennas) of the system. In this scenario, the scheduler constaint is alleviated because normally the number of concurrent beams allowed equals the total number of beams.

# Configuration file fields for analog beamforming

A set of parameters in configuration files controls the implementation of analog beamforming and instructs the scheduler on how to behave in such scenarios. Since most notably this technique in 5G is employed in FR2, the configuration file example currently available is a RFsim one for band 261. [Config file example](../ci-scripts/conf_files/gnb.sa.band261.u3.32prb.rfsim.conf)

In the `MACRLC` section of configuration files, there are three new parameters: `set_analog_beamforming`, `beam_duration` and `beams_per_period`. The explanation of these parameters is here provided:
- `set_analog_beamforming` can be set a value equal to 1 or 2 to activate or 0 to desactivate analog beamforming (default value is 0)
- `beam_duration` is the number of slots (currently minimum duration of a beam) the scheduler is tied to a beam (default value is 1)
- `beams_per_period` is the number of concurrent beams the RU can handle in the beam duration (default value is 1)
- `beam_weights` is a vector field containing the set of beam indices to be provided by the OAI L1 to the RU is also required. In current implementation, the number of beam indices should be equal to the number of SSBs transmitted

Setting analog beamforming to 1 or 2 changes the way FAPI beam index is treated. By setting 1, we instruct L1 to look up in Hi-PHY preconfigured DBM beam index. By setting 2, we instruct L2 to directly signal to Lo-PHY the beam index (e.g. over 7.2x fronthaul).

DAS is enabled by setting to 1 the parameter `enable_das` in the L1 section of the configuration file. In case of DAS enabled, the field `beam_weights` in `MACRLC` section can be omitted.

# Implementation in OAI scheduler

A new MAC structure `NR_beam_info_t` controls the behavior of the scheduler in presence of analog beamforming. Besides the already mentioned parameters `beam_duration` and `beams_per_period`, the structure also holds a matrix `beam_allocation[i][j]`, whose indices `i` and `j` stands respectively for the number of beams in the period and the slot index (the size of the latter depends on the frame characteristics).
This matrix contains the beams already allocated in a given slot, to flag the scheduler to use one of these to schedule a UE in one of these beams. If the matrix is full (all the beams in the given period, e.g. slot) are already allocated, the scheduler can't allocate a UE in a new beam.
To this goal, we extended the virtual resource block (VRB) map by one dimension to also contain information per allocated beam. As said, the scheduler can independently schedule users in a number of beams up to `beams_per_period` concurrently.

It is important to note that in current implementation, there are several periodical channels, e.g. PRACH or PUCCH for CSI et cetera, that have the precendence in being assigned a beam, that is because the scheduling is automatic, set in RRC configuration, and not up to the scheduler. For these instances, we assume the beam is available (if not there are assertions to stop the process). For data channels, the currently implemented PF scheduler is used. The only modification is that a UE can be served only if there is a free beam available or the one of the beams already in use correspond to that UE beam.

# FAPI implementation

To be noted that in our implementation analog beamforming is only supported in non-split/monolithic mode because we don't support yet SCF P19 interface that would be needed to manage these procedure in a split scenario with SCF FAPI.

In `config_request` structure, a vendor extension (`nfapi_nr_analog_beamforming_ve_t`) configures the lower layers at initialization with the following information:
- `analog_bf_vendor_ext` which can assume values 1 or 0 for enabling or disabling analog beamforming
- `num_beams_period_vendor_ext` which corresponds to the configuration parameter `beams_per_period`
- `total_num_beams_vendor_ext` which corresponds to the number of beams configured in `beam_weights`
- `analog_beam_list` which contains the RU beamforming indices configured in `beam_weights`

Additionally, L2 provides in each channel FAPI message information about the beam index. Small Cell Forum (SCF) FAPI provides in its PHY API specifications for the channels only a field for digital beamforming as part of the `precoding_and_beamforming` stucture. Therefore without a better option, we are currently using that one to store the internal analog beamforming index. This is the index used internally by the code to progressively identify the beam with a value from 0 to `total_num_beams_vendor_ext`.

# L1 implementation

To handle multiple concurrent beams, the buffers containing Tx and Rx data in frequency domain (`txdataF` and `rxdataF`) have been extended by one dimension to contain multiple concurrent beams to be transmitted/received.
The function `beam_index_allocation`, called by every L1 channel, is responsible to match the FAPI analog beam index to the RU beam index and to store the latter `beam_id` structure, which allocates the beams per symbol, despite L2 only supporting beam change at slot level. At the same time, the function returns the concurrent beam index, to be used to store data in frequency domain buffers. While doing so, the function also checks if there is room for current beam in the list of concurrent beams, which should always be the case, if L2 properly allocated the channels.

In case of DAS, since each beam corresponds to a specific antenna port, the `beam_index_allocation` function is simplified in the sense that the beam index corresponds to the antenna port index of the frequency domain buffers.

# RU implementation

The implementation is still work in progress.

The first dimension of the Tx and Rx buffers that used to contain the number of Tx/Rx antennas, it is now extended to contain the number of parallel streams which is the number of antennas multiplied by the number of concurrent beams.

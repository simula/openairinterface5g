This document highlights some of the key aspects of the implementation for O-RAN AAL/DPDK BBDEV; for user documentation, please consult the [setup guide](../../../../../doc/LDPC_OFFLOAD_SETUP.md) instead.

## Overview

The current implementation supports the following BBDEV devices and respective DPDK versions:
- Xilinx T2 on DPDK20.11+
- Intel ACC100* on DPDK22.11+
- Intel ACC200 (VRB1) on DPDK22.11+

> Important notes: 
> - For the Intel ACC100, for DPDK22.11 and DPDK23.11, users are required to [patch](https://github.com/DPDK/dpdk/commit/fdde63a1dfc129d0a510a831aa98253b36a2a1cd) the ACC100's driver.
> - If you intend to use this implementation in an FHI7.2 setup, only the F-release and beyond is supported.

## Implementation Details

Below, we highlight some of the important details in this implementation.

### HARQ 

To maintain the HARQ buffers in the LDPC decoding process, the BBDEV either uses its internal memory (not visible to DPDK), or using external memory (i.e., DDR) managed by OAI. 
For the Xilinx T2 and Intel ACC100, they use the internal memory while the Intel ACC200 uses the DDR memory.

If internal memory is supported by the device, we will set `RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_IN_ENABLE` and `RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE`.

The following two fields are important for HARQ: `harq_combined_input` and `harq_combined_output`.

For the first transmission, there is no HARQ input.
Thus, the flag `RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE` is not set.
However, there will be a HARQ output, so we set the flag `RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE`.
We also set the corresponding fields for `harq_combined_output`.

For the second transmission (or subsequent retransmissions), we need to provide a HARQ input at the same time, there will also be a HARQ output.
Therefore, both fields, `harq_combined_input` and `harq_combined_output` must be set accordingly.

As HARQ requires the output of the previous transmission/ HARQ round as its input, we need to maintain a separate variable to hold the previous round's information. 
To do so, we define `harq_buffers` (which is a pointer to an array of size `num_harq_codeblock` allocated in the heap later) in the `active_device` struct (see the following) in our implementation.
```C
struct active_device {
  ...
  uint32_t num_harq_codeblock;
  /* Persistent data structure to keep track of HARQ-related information */
  // Note: This is used to store/keep track of the combined output information across iterations
  struct rte_bbdev_op_data *harq_buffers;
  ...
} active_dev;
```


In the following, we elaborate on how the HARQ-related fields are set.

#### Internal Memory

When using the internal memory, we need to specify the *offset* of the hardware's memory where we want to store the HARQ combined output, and read from it accordingly later.

In the current implementation, we use a fixed offset of 32K, i.e., this means that each block is 32K. 

An index is derived from a unique HARQ PID assigned by OAI, modulo `active_dev.num_harq_codeblock` (which is configurable by the user through `nrLDPC_coding_t2.num_harq_codeblock`).
Using that index, we then derived the corresponding offset in the memory (i.e., `harq_combined_offset`).

```C
...

// Calculate offset in the HARQ combined buffers
// Unique segment offset
uint32_t segment_offset = (nrLDPC_slot_decoding_parameters->TBs[h].harq_unique_pid * NR_LDPC_MAX_NUM_CB) + i;
// Prune to avoid shooting above maximum id
uint32_t pruned_segment_offset = segment_offset % active_dev.num_harq_codeblock;
// Segment offset to byte offset
uint32_t harq_combined_offset = pruned_segment_offset * LDPC_MAX_CB_SIZE;

...
```

This `harq_combined_offset` is then set for `harq_combined_input.offset` and `harq_combined_output.offset` to tell BBDEV where to read/write the HARQ buffers.

Especially for the Intel ACCs, when providing the HARQ input, the `harq_combined_input.length` must be provided. 
To do so, we maintain the length of previous round's output using `harq_buffers` by copying it in `retrieve_ldpc_dec_op`.

#### External Memory

Different from the Xilinx T2 and Intel ACC100, the Intel ACC200 uses DDR memory to maintain the HARQ buffers which is managed by OAI.

In short, we use `harq_buffers` from the `active_device` struct, which have been initialzed with corresponding DPDK mempools for BBDEV to read/write the HARQ input/outputs.

For indexing, we use the `pruned_segment_offset`, which is derived from the unique HARQ PID assigned by OAI, modulo `active_dev.num_harq_codeblock` (which is configurable by the user through `nrLDPC_coding_t2.num_harq_codeblock`).

`harq_combined_input.offset` and `harq_combined_output.offset` is always set to 0 in our implementation.
Instead, we provide BBDEV the pointers to our allocated mempool regions through `harq_combined_input.data` and `harq_combined_output.data`.

In `retrieve_ldpc_dec_op`, we perform a `rte_memcpy` to copy the HARQ combined outputs to `harq_buffers`, along with other necessary metadata information to prepare for subsequent HARQ rounds.
Similar to the Intel ACC100, when providing the HARQ input, the `harq_combined_input.length` must be provided. 

### LLR Scaling

The LLR output from OAI's demodulation block is in 16 bits.
However, accelerators typically assume that the LLR inputs are scaled down using fixed-point arithmetic, e.g., 8 bits.

For the Xilinx T2, saturation is performed.
However, our tests have shown that this implementation did not work well for the Intel ACCs.

To that end, we perform LLR scaling differently for the Intel ACCs.
Through the function `llr_scaling`, we look at the min and max LLR values, perform scaling and subsequently saturation accordingly.

The fixed-point representations supported by the hardware is defined by the `llr_size` and `llr_decimal`.
For example, if `llr_size=8`, and `llr_decimal=1`, it means that we use a S8.1 representation (1 fractional bit), the min and max values are -64 to 63.5.

> *Limitation:* 
> We observe that in certain configurations, such as when using 2 MIMO layers, the LLR (Log-Likelihood Ratio) outputs exhibit a much wider dynamic range (e.g., from -10,000 to 10,000) compared to the typical sub-1,000 range seen in most other cases. 
> This suggests potential inconsistencies in the LLR values generated by the demodulator. 
> We believe that the decoding performance, when compared to the CPU implementation under the same number of LDPC decoding iterations, could be improved by refining the LLR scaling approach or revisiting the LLR generation process during demodulation. 
> This remains an area of active investigation.
>
> *Workaround:*
> As a temporary measure, increasing the number of LDPC decoding iterations—e.g., to 150—has shown to yield a decoding success rate comparable to that of the CPU implementation. 
> This comes at a negligible increase in decoding time based on our current evaluation.

### Special Case(s)

#### TB mode

For the T2, only CB mode is supported.

For the Intel ACCs, while we also use CB mode mostly, there is a special case that must be handled properly as specified by the BBDEV documentations: 
```
... The case when one CB belongs to TB and is being enqueued individually to BBDEV, this case is considered as a special case of partial TB where its number of CBs is 1. Therefore, it requires to get processed in TB-mode. ...
```

In such a case, we perform a check on whether there is only one TB, AND if the TB has only one CB.
If so, this has to be handled separately by using TB mode.

### Future Work(s)

#### TB mode

The current channel coding interface assumes LDPC encoding and decoding are performed on a per-code block basis.
However, to fully leverage Transport Block (TB) mode as supported in BBDEV, which can improve efficiency, the L1 implementation surrounding channel coding needs to be refactored. 
This remains an area for future investigation.

#### CRC24A/CRC16

CRC24A/CRC16 is supported by the BBDEV device (i.e., Intel ACC200). 
However, we do not make use of them and rely on OAI's existing CRC functions.
To use of BBDEV's CRC24A/CRC16 capability will be looked into in future MRs.

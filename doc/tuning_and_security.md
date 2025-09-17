This document outlines some information specific to system tuning for
running all executables (`nr-softmodem`, `lte-softmodem`, `nr-uesoftmodem`,
`lte-uesoftmodem`, `nr-cuup`, called softmodems for short in the following).
Also, it explains which Linux capabilities are required to run, and how to run
without sudo.

[TOC]

# Performance Tuning

Please also refer to the [advanced configuration in the
tutorial](NR_SA_Tutorial_COTS_UE.md#6-advanced-configurations-optional), which
groups many tips and tricks.

## CPU

OAI used to try to set the minimum CPU-DMA latency to 2 us by writing to
`/dev/cpu_dma_latency`. However, it is unclear if this has a significant
effect. Further, in containerized workloads, it might not be possible to set
this at all. We assume the user of OAI sets this value before starting OAI.
See the [Linux kernel
documentation](https://www.kernel.org/doc/html/latest/admin-guide/pm/cpuidle.html)
for more information.

OAI used to try to set the minimum frequency of cores running OAI to the
maximum frequency. We assume that the user sets CPU frequency policies
accordingly; in fact, tutorials generally suggest to either set a performance
CPU governor, or disable any sleep states at all. Hence, setting these
low-level parameters seems useless, and we assume the user of OAI disables
sleep states before starting OAI.

To disable all sleep states, simply run the following:
```
sudo cpupower idle-set -D0
```
Sometimes, dependencies might not be installed, in which case you should
install what `cpupower` asks you to install. Note that this is not persistent.

See the [Linux kernel
documentation](https://www.kernel.org/doc/html/latest/admin-guide/pm/cpufreq.html)
for more information.

You can disable hyper-threading in the BIOS.

You can disable KPTI Protections for Spectre/Meltdown for more performance.
**This is a security risk.** Add `mitigations=off nosmt` in your grub config and
update grub.

## Ethernet-based Radios

For ethernet-based radios, such as AW2S, some USRPs, and 7.2 radios, increase
the ringbuffers to a maximum. Read on interface `<fronthaul-interface-name>`
using option `-g`, then set it with `-G`:

```
ethtool -g <fronthaul-interface-name>
ethtool -G <fronthaul-interface-name> rx <maximum-rx-value> tx <maixmum-tx-value>
```

Also, you can increase the kernel's default and maximum read and write socket
buffer sizes to a high values, e.g., 134217728:

```
sudo sysctl -n -e -q -w net.core.rmem_default=134217728
sudo sysctl -n -e -q -w net.core.rmem_max=134217728
sudo sysctl -n -e -q -w net.core.wmem_default=134217728
sudo sysctl -n -e -q -w net.core.wmem_max=134217728
```

# Capabilities

Historically, all softmodems are executed as `root`, typically using `sudo`.
This remains a possibility, but we do not recommend it due to security
considerations. Rather, consider giving specific capabilities as outlined
below. Read `capabilities(7)` (`man 7 capabilities`) for more information on
each of the below capabilities.

Note that we tested this using 5G executables; 4G should work, but have not
been tested as extensively. If in doubt, run eNB/lteUE using `sudo`. The below
comments on capabilities apply in general as well; however, 4G executable might
not warn about missing capabilities or just fail.

Refer to any of the docker-compose files under `ci-scripts/` to see how to give
capabilities in docker. If you run from source, you can use `setcap` to mark a
process to run with specific capabilities. For instance, you can add the "general
capabilities" as described further below on the files like this:

```
sudo setcap cap_sys_nice+ep ./nr-softmodem
sudo setcap cap_ipc_lock+ep ./nr-softmodem
sudo setcap cap_sys_nice+ep ./nr-uesoftmodem
sudo setcap cap_ipc_lock+ep ./nr-uesoftmodem
sudo setcap cap_net_admin+ep ./nr-uesoftmodem
```

To make only temporary changes to capabilities, use `capsh`. It needs to be
started with all capabilities, so needs `sudo`. To drop all capabilities,
issue:
```
sudo -E capsh --drop="cap_sys_nice,cap_chown,cap_dac_read_search,cap_fowner,cap_fsetid,\
cap_kill,cap_setgid,cap_setuid,cap_setpcap,cap_linux_immutable,cap_net_bind_service,\
cap_net_broadcast,cap_net_admin,cap_net_raw,cap_ipc_lock,cap_ipc_owner,cap_sys_module,\
cap_sys_rawio,cap_sys_chroot,cap_sys_ptrace,cap_sys_pacct,cap_sys_admin,cap_sys_boot,\
cap_sys_resource,cap_sys_time,cap_sys_tty_config,cap_mknod,cap_lease,cap_audit_write,\
cap_audit_control,cap_setfcap,cap_mac_override,cap_mac_admin,cap_syslog,cap_wake_alarm,\
cap_block_suspend,cap_audit_read,cap_perfmon,cap_bpf,cap_checkpoint_restore" --print -- \
  -c "/absolute/path/to/nr-softmodem -O /absolute/path/to/config.conf"
```

(For readability, the command has been separated onto multiple lines through
`\`). To run with `SYS_NICE`, remove the first capability (`cap_sys_nice`)
from the list of dropped capabilities.

## General capabilities

- `SYS_NICE`: required by all softmodems to assign a higher priority to
  threads to increase the likelihood that the Linux scheduler schedules a
  thread for CPU time. Also, in some configurations, CPU pinning is employed to
  ensure consistent performance, also known as setting a CPU affinity.

  This capability is necessary by default when running any softmodem, for
  setting real-time requirements on processing threads. To allow to run without
  these requirements, the softmodems check if `SYS_NICE` is available, and
  skips any thread priority and affinity settings if the capability is not
  available. This allows to run any softmodem without root privileges in RFsim;
  you can see this by either observing a corresponding warning at the beginning
  of the execution, or the fact that no affinity/default priority is set for
  new threads.

- `IPC_LOCK`: OAI tries to lock all its virtual address space into RAM,
  preventing that memory from being paged to the swap area. If this capability
  is not present, a warning is printed.

- `NET_ADMIN`: Required at the UE to open the interface `oaitun_ue1` or eNB/gNB
  in noS1 mode. 5G executables will throw an error if they cannot create or
  modify an interface, but will continue running. It is therefore possible to
  run without this capability, but you cannot inject any traffic into the
  system. 4G executables might need this requirement, and possibly fail.

## Capabilities with DPDK

Additionally to the "general capabilities" above, you need these capabilities
when running with 7.2 fronthaul, which uses the xran library with a dependency
on DPDK:

- `IPC_LOCK` (becomes mandatory with DPDK)
- `SYS_RESOURCE`
- `NET_RAW`
- `SYS_ADMIN`: This is required by DPDK when using IOVA PA (Physical Address)
   mode to read `/proc/self/pagemaps`. However, if DPDK EAL is configured to use
   IOVA VA (Virtual Address) mode, this capability is no longer required.

## Capabilities with UHD

You don't need any additional capabilities for UHD beyond the "general
capabilities" for performance outlined above. Make sure that the USB device(s)
are readable and writable by the current user (e.g., `uhd_usrp_probe` can be
executed by a non-root user).

## Capabilities with AW2S

You don't need any additional capabilities for AW2S beyond the "general
capabilities" for performance outlined above.

## Other radios

Other radios have not been tested. If they do not work without additional
capabilities beyond the "general capabilities", please file a bug report.

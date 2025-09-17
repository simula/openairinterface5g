set -e
sudo cpupower idle-set -E > /dev/null
sudo sysctl kernel.sched_rt_runtime_us=950000
sudo sysctl kernel.timer_migration=1
exit 0

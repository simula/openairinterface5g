#!/bin/python3
import os
import subprocess
import numpy as np
import re
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import pickle
import argparse
import concurrent.futures
import traceback
import multiprocessing

default_ulsim_cmd = "../../cmake_targets/ran_build/build/nr_ulsim  -i 1,0 -t 100 -R 273"
ulsim_args = "-n {} -s {} -S {} -m {} -r {}"

def find_throughput(line):
    ### Example line
    ### SNR 10.000000: Channel BLER (0.000000e+00,-nan,-nan,-nan Channel BER (1.548551e-03,-nan,-nan,-nan) Avg round 1.00, Eff Rate 1672.0000 bits/slot, Eff Throughput 100.00, TBS 1672 bits/slot
    ### SNR -0.000000: Channel BLER (0.000000e+00,-nan,-nan,-nan Channel BER (1.888043e-01,-nan,-nan,-nan) Avg round 1.00, Eff Rate 2152.0000 bits/slot, Eff Throughput 100.00, TBS 2152 bits/slot
    pattern = r"SNR (-?\d+.\d+).*BLER \((-?\d+\.\d*(?:[eE][+-]\d+)?),.*Eff Throughput (\d+.\d+)"
    match = re.search(pattern, line)
    if match:
        snr, bler, thp = match.groups()
        return float(snr), float(bler), float(thp)
    return None, None, None

def generate_mcs_thps(ulsim_cmd_with_args, snr, mcs, num_runs = 10, num_rbs = 50, cache = {}):
    thp = 0
    bler = 0
    num_runs_cached = 0
    cache_string = "{},{},{}".format(snr, mcs, num_rbs)
    if cache_string in cache:
        thp, bler, num_runs_cached = cache[cache_string]
        if num_runs_cached >= num_runs:
            return bler, thp
    
    num_runs_left = num_runs - num_runs_cached
    
    ulsim_cmd_formatted = ulsim_cmd_with_args.format(num_runs_left, snr, snr+0.01, mcs, num_rbs)
    print(ulsim_cmd_formatted, "result insufficient in cache, running")
    process = subprocess.Popen(ulsim_cmd_formatted.split(" "), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout_data = process.communicate()[0]
    output = ""
    if stdout_data is not None:
        output = stdout_data.decode()
    for line in output.splitlines():
        _snr, _bler, _thp = find_throughput(line)
        if _snr is not None:
            bler = (bler * num_runs_cached + _bler * num_runs_left) / num_runs
            thp = (thp * num_runs_cached + _thp * num_runs_left) / num_runs
            cache[cache_string] = thp, bler, num_runs
            return bler, thp
    print("error processing "+ ulsim_cmd_with_args)
    print(output)
    print(process.communicate()[1].decode())
    return None, None



parser = argparse.ArgumentParser(description="UL SNR/BLER plot generator")
parser.add_argument("--snr-start", help="Starting SNR value")
parser.add_argument("--snr-stop", help="Final SNR value")
parser.add_argument("--snr-step", help="SNR step value")
parser.add_argument("--mcs-start", help="First MCS to test")
parser.add_argument("--mcs-stop", help="Last MCS to test")
parser.add_argument("--num-runs", help="Number of simulations per SNR/MCS value")
parser.add_argument("--rerun", help="Do not use cache to read data, run from scrach", action="store_true")
parser.add_argument("--num-rbs", help="Number of RBs")
parser.add_argument("--ulsim-cmd", help="ULSIM command to run. Do not use `sSrnm` flags, as these are used by the script", default=default_ulsim_cmd)

cpu_count = multiprocessing.cpu_count()
cpu_to_use = max(1, cpu_count - 2)

parser.add_argument("--num-threads", help="Number of threads to run", default=cpu_to_use)
args = parser.parse_args()

_snr_start = float(args.snr_start)
_snr_step = float(args.snr_step)
_snr_end = float(args.snr_stop) + _snr_step/2
_mcs_start = int(args.mcs_start)
_mcs_stop = int(args.mcs_stop)
_num_runs = int(args.num_runs)
_num_rbs = int(args.num_rbs)
ulsim_cmd = args.ulsim_cmd + " " + ulsim_args
num_threads = int(args.num_threads)

cmap = cm.viridis
plt.figure()
plt.ylim(-0.1, 1.1)
plt.title("BLER vs SNR for MCS{} to MCS{}, nRB={} (num_runs = {})".format(_mcs_start, _mcs_stop, _num_rbs, _num_runs))
plt.xlabel("SNR [dB]")
plt.ylabel("BLER")

cache = {}
if not args.rerun:
    try:
        with open("cache.pkl", "rb+") as f:
            cache = pickle.load(f)
    except FileNotFoundError:
        pass # no cache found

snrs_list = np.arange(_snr_start, _snr_end, _snr_step)
mcss_list = np.arange(_mcs_start, _mcs_stop + 1, 1)
plot_index = 0
for mcs in mcss_list:
    results = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=num_threads) as executor:
        result_to_snr_index = {executor.submit(generate_mcs_thps, ulsim_cmd, snrs_list[snr_index], mcs, _num_runs, _num_rbs, cache): snr_index for (snr_index, _) in enumerate(snrs_list)}
        for future in concurrent.futures.as_completed(result_to_snr_index):
            snr_index = result_to_snr_index[future]
            bler, thp = future.result()
            results[snr_index] = bler
    blers = np.zeros_like(snrs_list)
    for snr_index in results:
        blers[snr_index] = results[snr_index]

    num_plots = len(mcss_list)
    percentage = float(plot_index) / max(1, (num_plots - 1))
    color = cmap(percentage)
    plot_index += 1
    plt.plot(snrs_list, blers, label="MCS{}".format(mcs), color = color)
    plt.legend()
    plt.savefig('MCS={}-{},SNR={}-{},RBs={}graph.png'.format(_mcs_start, _mcs_stop, _snr_start, _snr_end, _num_rbs))
if not args.rerun:
    with open("cache.pkl", "wb+") as f:
        pickle.dump(cache, f)


# RF emulator library

The RF emulator is a simple network stack terminator on the RF side.  
It is meant to run the network stack without real radio and without RF simulation.
It simply drops TX signal and generate noise as an RX signal.
It can be used in `phy-test` mode to simply benchmark the network stack in a determined scenario.  
It synchronizes the network stack on the real time clock of the host to guarantee a realistic behavior of the network stack.
It detects and report late samples reads and writes to assess the real time behavior of the network stack.

## Building

`librf_emulator.so` is built by default by the `build_oai.sh` script.  
It is then located in the root build directory (usually `cmake_targets/ran_build/build`).

## Usage

### Enabling `rf_emulator`

Use the option `--device.name rf_emulator` of the softmodem to use the RF emulator.

### Options

Two options are available to configure the RF emulator:
* `--rf_emulator.enable_noise <enable>` Enables (enable = 1) or disables (enable = 0) noise generation on RX. By default noise generation is enabled.
* `--rf_emulator.noise_level_dBFS <noise_level>` Sets the level of noise generated in dB. If omitted no noise is generated.

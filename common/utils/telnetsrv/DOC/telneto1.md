[[_TOC_]]

The telnet O1 module (`telnetsrv_o1.c`) can be used to perform some O1-related
actions (reading data, starting and stopping the nr-softmodem, reconfigurating
frequency and bandwidth).

# General usage

The usage is similar to the general telnet usage, but in short:
```
./build_oai --ninja -c --gNB --nrUE --build-lib telnetsrv
```
to build everything including the telnet library.  Then, run the nr-softmodem
by activating telnet and loading the `o1` module:
```
./nr-softmodem -O <config> --telnetsrv --telnetsrv.shrmod o1
```

Afterwards, it should be possible to connect via telnet on localhost, port
9090. Use `help` to get help on the different command sections, and type e.g.
`o1 stats` to get statistics (more information further below):

```
$ telnet 127.0.0.1 9090
Trying 127.0.0.1...
Connected to 127.0.0.1.
Escape character is '^]'.

softmodem_gnb> help
[...]
   module 4 = o1:
      o1 stats
      o1 config ?
      o1 stop_modem
      o1 start_modem
[...]
softmodem_gnb> o1 stats
[...]
softmodem_gnb> exit
Connection closed by foreign host.
```

It also possible to send a command "directly from the command line", by piping
the command into netcat:
```
echo o1 stats | nc -N 127.0.0.1 9090
```

Note that only one telnet client can be connected at a time.

# Get statistics

Use the `o1 stats` command. The output is in JSON format:
```json
{
  "o1-config": {
    "BWP": {
      "dl": [
        {
          "bwp3gpp:isInitialBwp": true,
          "bwp3gpp:numberOfRBs": 106,
          "bwp3gpp:startRB": 0,
          "bwp3gpp:subCarrierSpacing": 30
        }
      ],
      "ul": [
        {
          "bwp3gpp:isInitialBwp": true,
          "bwp3gpp:numberOfRBs": 106,
          "bwp3gpp:startRB": 0,
          "bwp3gpp:subCarrierSpacing": 30
        }
      ]
    },
    "NRCELLDU": {
      "nrcelldu3gpp:ssbFrequency": 641280,
      "nrcelldu3gpp:arfcnDL": 640008,
      "nrcelldu3gpp:bSChannelBwDL": 40,
      "nrcelldu3gpp:arfcnUL": 640008,
      "nrcelldu3gpp:bSChannelBwUL": 40,
      "nrcelldu3gpp:nRPCI": 0,
      "nrcelldu3gpp:nRTAC": 1,
      "nrcelldu3gpp:mcc": "208",
      "nrcelldu3gpp:mnc": "95",
      "nrcelldu3gpp:sd": 16777215,
      "nrcelldu3gpp:sst": 1
    },
    "device": {
      "gnbId": 1,
      "gnbName": "gNB-Eurecom-5GNRBox",
      "vendor": "OpenAirInterface"
    }
  },
  "O1-Operational": {
    "frame-type": "tdd",
    "band-number": 78,
    "num-ues": 1,
    "ues": [
      6876
    ],
    "load": 9,
    "ues-thp": [
      {
        "rnti": 6876,
        "dl": 3279,
        "ul": 2725
      }
    ]
  }
}
```

Note that no actual JSON engine is used, so no actual verification is done; it
is for convenience of the consumer. To verify, you can employ `jq`:
```
echo o1 stats | nc -N 127.0.0.1 9090 | awk '/^{$/, /^}$/' | jq .
```
(`awk`'s pattern matching makes that only everything between the first `{` and
its corresponding `}` is printed).

There are two sections:
1. `.o1-config` show some stats that map directly to the O1 Netconf model. Note
   that only one MCC/MNC/SD/SST (each) are supported right now. Also, note that
   as per 3GPP specifications, SD of value `0xffffff` (16777215 in decimal)
   means "no SD". `bSChannelBwDL/UL` is reported in MHz.
2. `.O1-operational` output some statistics that do not map yet to any netconf
   parameters, but that might be useful nevertheless for a consumer.

# Write a new configuration

Use `o1 config` to write a configuration:
```
echo o1 config nrcelldu3gpp:ssbFrequency 620736 nrcelldu3gpp:arfcnDL 620020 nrcelldu3gpp:bSChannelBwDL 51 bwp3gpp:numberOfRBs 51 bwp3gpp:startRB 0 | nc -N 127.0.0.1 9090
```
You have to pass the above parameters in exactly this order. The softmodem
needs to be stopped; it will pick up the new configuration when starting the
softmodem again.

Note that you cannot switch three-quarter sampling for this as of now.

For values of the configuration, refer to the next section.

# Use hardcoded configuration

Use `o1 bwconfig` to write a hard-coded configuration for 20 or 40 MHz cells:
```
echo o1 bwconfig 20 | nc -N 127.0.0.1 9090
echo o1 bwconfig 40 | nc -N 127.0.0.1 9090
```

The softmodem needs to be stopped; it will pick up the new configuration when
starting the softmodem again.

Use `o1 stats` to see which configurations are set by these commands for the
parameters `nrcelldu3gpp:ssbFrequency`, `nrcelldu3gpp:arfcnDL`,
`nrcelldu3gpp:arfcnUL`, `nrcelldu3gpp:bSChannelBwDL`,
`nrcelldu3gpp:bSChannelBwUL`, and `bwp3gpp:numberOfRBsbwp3gpp:startRB`.
Furthermore, for 20MHz, it *disables* three-quarter sampling, whereas it
*enables* three-quarter sampling for 40MHz.

# Restart the softmodem

Use `o1 stop_modem` to stop the `nr-softmodem`. To restart the softmodem, use
`o1 start_modem`:
```
echo o1 stop_modem | nc -N 127.0.0.1 9090
echo o1 start_modem | nc -N 127.0.0.1 9090
```

In fact, stopping terminates all L1 threads. It will be as if the softmodem
"freezes", and no periodical output of statistics will occur (the O1 telnet
interface will still work, though). Starting again will "defreeze" the
softmodem.

Upon restart, the DU sends a gNB-DU configuration update to the CU to inform it
about the updated configuration. Therefore, this also works in F1.

#!/bin/bash

set -uo pipefail

PREFIX=/opt/oai-gnb-aw2s
CONFIGFILE=$PREFIX/etc/gnb.conf

echo "=================================="
echo "/proc/sys/kernel/core_pattern=$(cat /proc/sys/kernel/core_pattern)"

if [ ! -f $CONFIGFILE ]; then
  echo "No configuration file $CONFIGFILE found: attempting to find YAML config"
  YAML_CONFIGFILE=$PREFIX/etc/gnb.yaml
  if [ ! -f $YAML_CONFIGFILE ]; then
    echo "No configuration file $YAML_CONFIGFILE found. Please mount either at $CONFIGFILE or $YAML_CONFIGFILE"
    exit 255
  fi
  CONFIGFILE=$YAML_CONFIGFILE
fi

echo "=================================="
echo "== Configuration file:"
cat $CONFIGFILE

new_args=()

while [[ $# -gt 0 ]]; do
  new_args+=("$1")
  shift
done

new_args+=("-O")
new_args+=("$CONFIGFILE")

# enable printing of stack traces on assert
export OAI_GDBSTACKS=1

echo "=================================="
echo "== Starting gNB soft modem with AW2S"
if [[ -v USE_ADDITIONAL_OPTIONS ]]; then
    echo "Additional option(s): ${USE_ADDITIONAL_OPTIONS}"
    while [[ $# -gt 0 ]]; do
        new_args+=("$1")
        shift
    done
    for word in ${USE_ADDITIONAL_OPTIONS}; do
        new_args+=("$word")
    done
    echo "${new_args[@]}"
    exec "${new_args[@]}"
else
    echo "${new_args[@]}"
    exec "${new_args[@]}"
fi

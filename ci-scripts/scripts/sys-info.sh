CORES=$(nproc)
MODEL=$(lscpu | awk -F: '/Model name/ {print $2; exit}' | xargs)
MAX_FREQ=$(lscpu | awk -F: '/CPU max MHz/ {printf "%.2f MHz", $2; exit}')
CURRENT_FREQ=$(awk -F: '/cpu MHz/ {printf " %.2f MHz", $2; exit}' /proc/cpuinfo)
RAM=$(free -h --si | awk '/Mem:/ {print $2}')
# sudo -n: non-interactive, will silently fail in case we don't have privileges
DMID=$(sudo -n dmidecode -t memory 2>/dev/null)
if [ $? -eq 0 ]; then
  RAM_TYPE=$(echo "$DMID" | awk -F: '/Type:/ {print $2}' | grep -Ev 'Unknown|Other|None' | head -1 | xargs)
else
  RAM_TYPE="can't query"
fi


echo "CPU: model ${MODEL}"
echo "${CORES} cores, max CPU freq ${MAX_FREQ}, current ${CURRENT_FREQ}"
echo "RAM ${RAM} type ${RAM_TYPE}"

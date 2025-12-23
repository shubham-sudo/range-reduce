#!/bin/bash
# Script for measuring CPU, memory and IO device utilization
# Author: Manos Athanassoulis

if [ "$#" -lt 4 ]; then
    echo "Usage: $0 <arg1> <arg2> <arg3> <arg4>"
    exit 1
fi

CPU_IOSTATOUT=$(mktemp __iostat_out.XXXXXXXXXX)
DISK_IOSTATOUT=$(mktemp __iostat_disk_out.XXXXXXXXXX)
TOPOUT=$(mktemp __top_out.XXXXXXXXXX)

DEVICE=/dev/sda
CPU_INTERVAL=1
CPU_TOP_INTERVAL=00.05
DISK_INTERVAL=1

ARGS=("$@")

# ----------------------------------------------------------------------
iostat -t -c ${CPU_INTERVAL} >> ${CPU_IOSTATOUT}&
CPU_IOSTATPID=$!
iostat -t -d ${DISK_INTERVAL} -x -k ${DEVICE}>> ${DISK_IOSTATOUT}&
DISK_IOSTATPID=$!

# Run the track_io script with its arguments
../../working_version "${ARGS[@]}" > "workload.log" &
APP_PID=$!

top -b -p ${APP_PID} -d ${CPU_TOP_INTERVAL} >> ${TOPOUT}&
TOP_PID=$!

wait ${APP_PID}
kill ${CPU_IOSTATPID}
kill ${DISK_IOSTATPID}
kill ${TOP_PID}

sleep 1
echo Printing results
echo
echo "CPU from iostat (every $DISK_INTERVAL s)"
cat "${CPU_IOSTATOUT}" | head -n 4 | tail -n 1 >> "cpu_iostat.txt"
cat "${CPU_IOSTATOUT}" | grep "avg-cpu" -A 1 | grep -v "avg-cpu" | awk '{if ($1!="--") print $0}' >> "cpu_iostat.txt"
rm "${CPU_IOSTATOUT}"
echo
echo "CPU from top (every $CPU_TOP_INTERVAL s)"
cat "${TOPOUT}" | head -n 7 | tail -n 1 >> "cpu_top.txt"
cat "${TOPOUT}" | grep ${APP_PID} >> "cpu_top.txt"
rm "${TOPOUT}"
echo
echo "IO from iostat (every $DISK_INTERVAL s)"
cat "${DISK_IOSTATOUT}" | head -n 4 | tail -n 1 >> "io_iostat.txt"
cat "${DISK_IOSTATOUT}" | grep "Device" -A 1 | grep -v "Device" | awk '{if ($1!="--") print $0}' >> "io_iostat.txt"
rm "${DISK_IOSTATOUT}"

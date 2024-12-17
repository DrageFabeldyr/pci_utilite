#!/bin/bash

#sudo sh -c "echo 0000:03:00.0 > /sys/bus/pci/drivers/serial/unbind"
#sudo sh -c "echo 0000:03:00.0 > /sys/bus/pci/drivers/mypcidriver/bind"
#sudo sh -c "echo 0000:03:02.0 > /sys/bus/pci/drivers/serial/unbind"
#sudo sh -c "echo 0000:03:02.0 > /sys/bus/pci/drivers/mypcidriver/bind"

echo "change driver script"
# чтобы не вылетало с ошибкой, если файлов нет
shopt -s nullglob;

for device in /sys/bus/pci/drivers/serial/*; do
	echo "Device: $device"
	filename=$(basename "$device")
	echo "Filename: $filename"
	if [[ "$filename" == "0000:"* ]]; then
		echo "Found match: $filename"
		sudo sh -c "echo \"$filename\" > /sys/bus/pci/drivers/serial/unbind"
		sudo sh -c "echo \"$filename\" > /sys/bus/pci/drivers/mypcidriver/bind"
	else
		echo "No match: $filename"
	fi
done

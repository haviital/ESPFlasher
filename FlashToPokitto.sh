#!/bin/bash

date -r $HOME/bin/FemtoIDE/projects/ESPFlasher/ESPFlasher.bin
date 

dd bs=1024 conv=nocreat,notrunc if=$HOME/bin/FemtoIDE/projects/ESPFlasher/ESPFlasher.bin of=/media/$USER/CRP\ DISABLD/firmware.bin
read -t 5 -p "Finished!"

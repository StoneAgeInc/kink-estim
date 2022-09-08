#!/bin/bash

chmod 777 zappy_board/config/app_config_template.h
cp ./app_config_template.h ./zappy_board/config/app_config.h

CH0=${1:-1}
CH1=${2:-2}
CH2=${3:-3}
CH3=${4:-4}
PD=${5:-7000}
PW=${6:-140}
ID=${7:-0}

echo "#define CHANNEL_0_POWER $CH0" >> ./zappy_board/config/app_config.h
echo "#define CHANNEL_1_POWER $CH1" >> ./zappy_board/config/app_config.h
echo "#define CHANNEL_2_POWER $CH2" >> ./zappy_board/config/app_config.h
echo "#define CHANNEL_3_POWER $CH3" >> ./zappy_board/config/app_config.h
echo "#define DEFAULT_PULSE_DELAY $PD // in micro-seconds" >> ./zappy_board/config/app_config.h
echo "#define DEFAULT_PULSE_WIDTH $PW // in micro-seconds" >> ./zappy_board/config/app_config.h 
echo "#define DEFAULT_INTERPULSE_DELAY $ID // in micro-seconds, can be negative" >> ./zappy_board/config/app_config.h

cd zappy_board/build
make clean
make

echo "**************************************************"
echo ""
echo "Channel 0 : $CH0"
echo "Channel 1 : $CH1"
echo "Channel 2 : $CH2"
echo "Channel 3 : $CH3"
echo "Pulse Delay : $PD micro Seconds"
echo "Pulse Width : $PW micro Seconds"
echo "Interpulse Delay : $ID micro Seconds"
echo ""
echo "**************************************************"

make flash_zappy_board
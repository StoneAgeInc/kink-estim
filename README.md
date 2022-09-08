# kink-estim
Kink TENS repository

## Other Repos
	1. https://github.com/EntropicEngineering/libprv_nRF5
	2. https://github.com/lvgl/lvgl.git

### Build Steps
1. `cd zappy_board`
2. `mkdir -p build`
3. `cd build`
4. `cmake .. -DSDK_ROOT=<absolute SDK directory path>`
5. `make`

### Build and flash using Test Script
Run the build.sh bash script to make and flash zappy_board on the target.
Only works after first 4 steps of 'Build Steps' are done.

The script takes 7 command line arguments:
1. Channel 0 Power (Range 0 to 4095, default value 1)
2. Channel 1 Power (Range 0 to 4095, default value 1)
3. Channel 2 Power (Range 0 to 4095, default value 1)
4. Channel 3 Power (Range 0 to 4095, default value 1)
5. Pulse Delay in uS (Preferred value greater than 2000, default value 7000)
6. Pulse Width in uS (Preferred value between 1 and 200, default value 200)
7. Interpulse delay in uS (Preferred value 0, default value 0)

Run the script like:
`./build.sh $1 $2 $3 $4 $5 $6 $7`

1, 2, 3, 4, 5, 6, 7 correspond to arguments in respective pointers above

Sample
`./build.sh 100 500 1500 4000 5000 150 0`
This set the following parameters

1. Channel 0 Power: 100
2. Channel 1 Power: 500
3. Channel 2 Power: 1500
4. Channel 3 Power: 4000
5. Pulse Delay: 5000 uS)
6. Pulse Width: 150 uS
7. Interpulse delay: 0 uS

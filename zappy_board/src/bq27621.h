//
// Created by Benjamin Riggs on 6/1/21.
//

#ifndef BQ27621_H
#define BQ27621_H

#define BQ27621_

#define BQ27621_I2C_ADDRESS                         0x55
#define BQ27621_I2C_WAIT_TIME_ms                    1
#define BQ27621_DEVICE_TYPE_VALUE                   0x0621

#define BQ27621_COMMAND_CODE0_CNTL                  0x00
#define BQ27621_COMMAND_CODE1_CNTL                  0x01

// Subcommands sent big-endian
#define BQ27621_CNTL_FUNCTION_CONTROL_STATUS        0x0000
#define BQ27621_CNTL_FUNCTION_DEVICE_TYPE           0x0001

#endif //BQ27621_H

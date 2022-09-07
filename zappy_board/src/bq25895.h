//
// Created by Benjamin Riggs on 2/28/20.
//

#ifndef BQ25895_H
#define BQ25895_H

#include <stdbool.h>
#include <stdint.h>

#define BQ25895_I2C_ADDRESS 0x6A
#define BQ25895_STATUS_REG_COUNT 5

typedef enum {
    BQ25895_REG00,
    BQ25895_REG01,
    BQ25895_REG02,
    BQ25895_REG03,
    BQ25895_REG04,
    BQ25895_REG05,
    BQ25895_REG06,
    BQ25895_REG07,
    BQ25895_REG08,
    BQ25895_REG09,
    BQ25895_REG0A,
    BQ25895_REG0B,
    BQ25895_REG0C,
    BQ25895_REG0D,
    BQ25895_REG0E,
    BQ25895_REG0F,
    BQ25895_REG10,
    BQ25895_REG11,
    BQ25895_REG12,
    BQ25895_REG13,
    BQ25895_REG14,
} bq25895_register_t;

// REG00 Fields
#define BQ25895_EN_HIZ_MASK             0x80 // REG00 // Enable HIZ mode
#define BQ25895_EN_HIZ_SHIFT            7 // 0 – Disable (default)
#define BQ25895_EN_ILIM_MASK            0x40 // REG00 // Enable ILIM Pin
#define BQ25895_EN_ILIM_SHIFT           6 // 1 – Enable (default: Enable ILIM pin (1))
#define BQ25895_IINLIM_MASK             0x3F // REG00 // Input Current Limit, Offset: 100mA
#define BQ25895_IINLIM_SHIFT            0 // Default:0b001000 (500mA)
// REG01 Fields
#define BQ25895_BHOT_MASK               0x03 // REG01 // Boost Mode Hot Temperature Monitor Threshold
#define BQ25895_BHOT_SHIFT              6 // 00 – VBHOT1 Threshold (34.75%) (default)
#define BQ25895_BCOLD_MASK              0x20 // REG01 // Boost Mode Cold Temperature Monitor Threshold
#define BQ25895_BCOLD_SHIFT             5 // 0 – VBCOLD0 Threshold (Typ. 77%) (default)
#define BQ25895_VINDPM_OS_MASK          0x1F // REG01 // Input Voltage Limit Offset
#define BQ25895_VINDPM_OS_SHIFT         0 // Default: 600mV (00110)
// REG02 Fields
#define BQ25895_CONV_START_MASK         0x80 // REG02 // ADC Conversion Start Control
#define BQ25895_CONV_START_SHIFT        7 // This bit is read-only when CONV_RATE = 1. The bit stays high during ADC conversion and during input source detection.
#define BQ25895_CONV_RATE_MASK          0x40 // REG02 // ADC Conversion Rate Selection
#define BQ25895_CONV_RATE_SHIFT         6 // 0 – One shot ADC conversion (default) 1 – Start 1s Continuous Conversion
#define BQ25895_BOOST_FREQ_MASK         0x20 // REG02 // Boost Mode Frequency Selection
#define BQ25895_BOOST_FREQ_SHIFT        5 // 1 – 500KHz (default) Note: Write to this bit is ignored when OTG_CONFIG is enabled.
#define BQ25895_ICO_EN_MASK             0x10 // REG02 // Input Current Optimizer (ICO) Enable
#define BQ25895_ICO_EN_SHIFT            4 // 1 – Enable ICO Algorithm (default)
#define BQ25895_HVDCP_EN_MASK           0x08 // REG02 // High Voltage DCP Enable
#define BQ25895_HVDCP_EN_SHIFT          3 // 1 – Enable HVDCP handshake (default)
#define BQ25895_MAXC_EN_MASK            0x04 // REG02 // MaxCharge Adapter Enable
#define BQ25895_MAXC_EN_SHIFT           2 // 1 – Enable MaxCharge handshake (default)
#define BQ25895_FORCE_DPDM_MASK         0x02 // REG02 // Force D+/D- Detection
#define BQ25895_FORCE_DPDM_SHIFT        1 // 0 – Not in D+/D- or PSEL detection (default)
#define BQ25895_AUTO_DPDM_EN_MASK       0x01 // REG02 // Automatic D+/D- Detection Enable
#define BQ25895_AUTO_DPDM_EN_SHIFT      0 // 1 –Enable D+/D- or PEL detection when VBUS is plugged-in (default)
// REG03 Fields
#define BQ25895_BAT_LOADEN_MASK         0x80 // REG03 // Battery Load (IBATLOAD) Enable
#define BQ25895_BAT_LOADEN_SHIFT        7 // 0 – Disabled (default)
#define BQ25895_WD_RST_MASK             0x40 // REG03 // I2C Watchdog Timer Reset
#define BQ25895_WD_RST_SHIFT            6 // 0 – Normal (default) 1 – Reset (Back to 0 after timer reset)
#define BQ25895_OTG_CONFIG_MASK         0x20 // REG03 // Boost (OTG) Mode Configuration
#define BQ25895_OTG_CONFIG_SHIFT        5 // 1 – OTG Enable (default)
#define BQ25895_CHG_CONFIG_MASK         0x10 // REG03 // Charge Enable Configuration
#define BQ25895_CHG_CONFIG_SHIFT        4 // 1- Charge Enable (default)
#define BQ25895_SYS_MIN_MASK            0x0E // REG03 // Minimum System Voltage Limit
#define BQ25895_SYS_MIN_SHIFT           1 // Default: 3.5V (101)
// REG04 Fields
#define BQ25895_EN_PUMPX_MASK           0x80 // REG04 // Current pulse control Enable
#define BQ25895_EN_PUMPX_SHIFT          7 // 0 - Disable Current pulse control (default)
#define BQ25895_ICHG_MASK               0x7F // REG04 // Fast Charge Current Limit
#define BQ25895_ICHG_SHIFT              0 // Default: 2048mA (0100000)
// REG05 Fields
#define BQ25895_IPRECHG_MASK            0xF0 // REG05 // Precharge Current Limit
#define BQ25895_IPRECHG_SHIFT           4 // Default: 128mA (0001)
#define BQ25895_ITERM_MASK              0x0F // REG05 // Termination Current Limit
#define BQ25895_ITERM_SHIFT             0 // Default: 256mA (0011)
// REG06 Fields
#define BQ25895_VREG_MASK               0xFC // REG06 // Charge Voltage Limit
#define BQ25895_VREG_SHIFT              2 // Default: 4.208V (010111)
#define BQ25895_BATLOWV_MASK            0x02 // REG06 // Battery Precharge to Fast Charge Threshold
#define BQ25895_BATLOWV_SHIFT           1 // 1 – 3.0V (default)
#define BQ25895_VRECHG_MASK             0x01 // REG06 // Battery Recharge Threshold Offset (below Charge Voltage Limit)
#define BQ25895_VRECHG_SHIFT            0 // 0 – 100mV (VRECHG) below VREG (REG06[7:2]) (default)
// REG07 Fields
#define BQ25895_EN_TERM_MASK            0x80 // REG07 // Charging Termination Enable
#define BQ25895_EN_TERM_SHIFT           7 // 1 – Enable (default)
#define BQ25895_STAT_DIS_MASK           0x40 // REG07 // STAT Pin Disable
#define BQ25895_STAT_DIS_SHIFT          6 // 0 – Enable STAT pin function (default)
#define BQ25895_WATCHDOG_MASK           0x30 // REG07 // I2C Watchdog Timer Setting
#define BQ25895_WATCHDOG_SHIFT          4 // 01 – 40s (default)
#define BQ25895_EN_TIMER_MASK           0x08 // REG07 // Charging Safety Timer Enable
#define BQ25895_EN_TIMER_SHIFT          3 // 1 – Enable (default)
#define BQ25895_CHG_TIMER_MASK          0x06 // REG07 // Fast Charge Timer Setting
#define BQ25895_CHG_TIMER_SHIFT         1 // 10 – 12 hrs (default)
// REG08 Fields
#define BQ25895_BAT_COMP_MASK           0xE0 // REG08 // IR Compensation Resistor Setting
#define BQ25895_BAT_COMP_SHIFT          5 // Default: 0Ω (000) (i.e. Disable IRComp)
#define BQ25895_VCLAMP_MASK             0x16 // REG08 // IR Compensation Voltage Clamp
#define BQ25895_VCLAMP_SHIFT            2 // Default: 0mV (000)
#define BQ25895_TREG_MASK               0x03 // REG08 // Thermal Regulation Threshold
#define BQ25895_TREG_SHIFT              0 // 11 – 120°C (default)
// REG09 Fields
#define BQ25895_FORCE_ICO_MASK          0x80 // REG09 // Force Start Input Current Optimizer (ICO)
#define BQ25895_FORCE_ICO_SHIFT         7 // 0 – Do not force ICO (default)
#define BQ25895_TMR2X_EN_MASK           0x40 // REG09 // Safety Timer Setting during DPM or Thermal Regulation
#define BQ25895_TMR2X_EN_SHIFT          6 // 1 – Safety timer slowed by 2X during input DPM or thermal regulation (default)
#define BQ25895_BATFET_DIS_MASK         0x20 // REG09 // Force BATFET off to enable ship mode
#define BQ25895_BATFET_DIS_SHIFT        5 // 0 – Allow BATFET turn on (default)
#define BQ25895_BATFET_DLY_MASK         0x08 // REG09 // BATFET turn off delay control
#define BQ25895_BATFET_DLY_SHIFT        3 // 0 – BATFET turn off immediately when BATFET_DIS bit is set (default)
#define BQ25895_BATFET_RST_EN_MASK      0x04 // REG09 // BATFET full system reset enable
#define BQ25895_BATFET_RST_EN_SHIFT     2 // 1 – Enable BATFET full system reset (default)
#define BQ25895_PUMPX_UP_MASK           0x02 // REG09 // Current pulse control voltage up enable
#define BQ25895_PUMPX_UP_SHIFT          1 // 0 – Disable (default)
#define BQ25895_PUMPX_DN_MASK           0x01 // REG09 // Current pulse control voltage down enable
#define BQ25895_PUMPX_DN_SHIFT          0 // 0 – Disable (default)
// REG0A Fields
#define BQ25895_BOOSTV_MASK             0xF0 // REG0A // Boost Mode Voltage Regulation
#define BQ25895_BOOSTV_SHIFT            4 // Default: 5.126V (1001)
// REG0B Fields
#define BQ25895_VBUS_STAT_MASK          0xE0 // REG0B // VBUS Status register
#define BQ25895_CHRG_STAT_MASK          0x18 // REG0B // Charging Status
#define BQ25895_PG_STAT_MASK            0x04 // REG0B // Power Good Status
#define BQ25895_SDP_STAT_MASK           0x02 // REG0B // USB Input Status
#define BQ25895_VSYS_STAT_MASK          0x01 // REG0B // VSYS Regulation Status
// REG0C Fields
#define BQ25895_WATCHDOG_FAULT_MASK     0x80 // REG0C // Watchdog Fault Status
#define BQ25895_BOOST_FAULT_MASK        0x40 // REG0C // Boost Mode Fault Status
#define BQ25895_CHRG_FAULT_MASK         0x30 // REG0C // Charge Fault Status
#define BQ25895_BAT_FAULT_MASK          0x08 // REG0C // Battery Fault Status
#define BQ25895_NTC_FAULT_MASK          0x07 // REG0C // NTC Fault Status
// REG0D Fields
#define BQ25895_FORCE_VINDPM_MASK       0x80 // REG0D // VINDPM Threshold Setting Method
#define BQ25895_FORCE_VINDPM_SHIFT      7 // 0 – Run Relative VINDPM Threshold (default)
#define BQ25895_VINDPM_MASK             0x7F // REG0D // Absolute VINDPM Threshold
#define BQ25895_VINDPM_SHIFT            0 // Default: 4.4V (0010010)
// REG0E-REG13 Fields
#define BQ25895_THERM_STAT_MASK         0x80 // REG0E // Thermal Regulation Status
#define BQ25895_BATV_MASK               0x7F // REG0E // ADC conversion of Battery Voltage (VBAT)
#define BQ25895_SYSV_MASK               0x7F // REG0F // ADC conversion of System Voltage (VSYS)
#define BQ25895_TSPCT_MASK              0x7F // REG10 // ADC conversion of TS Voltage (TS) as percentage of REGN
#define BQ25895_VBUS_GD_MASK            0x80 // REG11 // VBUS Good Status
#define BQ25895_VBUSV_MASK              0x7F // REG11 // ADC conversion of VBUS voltage (VBUS)
#define BQ25895_ICHGR_MASK              0x7F // REG12 // ADC conversion of Charge Current (IBAT) when VBAT > VBATSHORT
#define BQ25895_VDPM_STAT_MASK          0x80 // REG13 // VINDPM Status
#define BQ25895_IDPM_STAT_MASK          0x40 // REG13 // IINDPM Status
#define BQ25895_IDPM_LIM_MASK           0x3F // REG13 // Input Current Limit in effect while Input Current Optimizer (ICO) is enabled
// REG14 Fields
#define BQ25895_REG_RST_MASK            0x80 // REG14 // Register Reset
#define BQ25895_REG_RST_SHIFT           7 // 0 – Keep current register setting (default)
#define BQ25895_ICO_OPTIMIZED_MASK      0x40 // REG14 // Input Current Optimizer (ICO) Status
#define BQ25895_PN_MASK                 0x38 // REG14 // Device Configuration,111: bq25895
#define BQ25895_TS_PROFILE_MASK         0x04 // REG14 // Temperature Profile
#define BQ25895_DEV_REV_MASK            0x03 // REG14 // Device Revision: 01

typedef enum {
    NO_INPUT = 0,
    USB_HOST_SPD = 1,
    USB_CPD = 2,
    UBS_DCP = 3,
    HV_DCP = 4,
    UNKNOWN_ADAPTER = 5,
    NONSTANDARD_ADAPTER = 6,
    USB_OTG = 7
} bq25895_VBUS_status_t;

typedef enum {
    NOT_CHARGING = 0,
    PRE_CHARGING = 1,
    FAST_CHARGING = 2,
    CHARGE_TERM_DONE = 3
} bq25895_charging_status_t;

typedef enum {
    NORMAL = 0,
    INPUT_FAULT = 1,
    THERMAL_SHUTDOWN = 2,
    CHARGE_SAFETY_TIMER_EXPIRATION = 3
} bq25895_charge_fault_status_t;

typedef enum {
    TS_NORMAL = 0,
    BUCK_MODE_TS_COLD = 1,
    BUCK_MODE_TS_HOT = 2,
    BOOST_MODE_TS_COLD = 5,
    BOOST_MODE_TS_HOT = 6
} bq25895_NTC_fault_status_t;

typedef struct __packed {
    bq25895_NTC_fault_status_t      NTC_FAULT       : 3;
    bool                            BAT_FAULT       : 1;
    bq25895_charge_fault_status_t   CHRG_FAULT      : 2;
    bool                            BOOST_FAULT     : 1;
    bool                            WATCHDOG_FAULT  : 1;
} bq25895_REG0C_t;

#endif //BQ25895_H

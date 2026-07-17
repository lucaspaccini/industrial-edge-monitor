#pragma once

/* Calibration registers */
#define BME280_REG_CALIB_T1           0x88
#define BME280_REG_CALIB_H1           0xA1
#define BME280_REG_CALIB_H2           0xE1

/* Device management registers */
#define BME280_REG_CHIP_ID            0xD0
#define BME280_REG_RESET              0xE0
#define BME280_REG_STATUS             0xF3

/* Configuration registers */
#define BME280_REG_CTRL_HUM           0xF2
#define BME280_REG_CTRL_MEAS          0xF4
#define BME280_REG_CONFIG             0xF5

/* Measurement registers */
#define BME280_REG_PRESS_MSB          0xF7

/* Device constants */
#define BME280_CHIP_ID                0x60
#define BME280_RESET_COMMAND          0xB6

/* Calibration block lengths */
#define BME280_CALIBRATION_BLOCK_1_SIZE   26
#define BME280_CALIBRATION_BLOCK_2_SIZE   7

/* Pressure + temperature + humidity raw data */
#define BME280_MEASUREMENT_DATA_SIZE      8

/* Status register masks */
#define BME280_STATUS_IM_UPDATE_MASK      0x01

/*
 * ctrl_hum:
 * osrs_h = 001 -> humidity oversampling x1
 */
#define BME280_CTRL_HUM_VALUE         0x01

/*
 * config:
 * t_sb   = 101 -> 1000 ms standby
 * filter = 000 -> disabled
 * spi3w  = 0
 */
#define BME280_CONFIG_VALUE           0xA0

/*
 * ctrl_meas:
 * osrs_t = 001 -> temperature oversampling x1
 * osrs_p = 001 -> pressure oversampling x1
 * mode   = 11  -> normal mode
 */
#define BME280_CTRL_MEAS_VALUE        0x27
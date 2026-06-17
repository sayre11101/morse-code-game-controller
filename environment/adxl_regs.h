#ifndef ADXL_REGS_H
#define ADXL_REGS_H

/* ADXL345 default I2C address when ALT_ADDRESS is tied to GND */
#define ADXL345_ADDR 0x53

/* Register map */
#define ADXL345_REG_DEVID       0x00
#define ADXL345_REG_OFSX        0x1E
#define ADXL345_REG_OFSY        0x1F
#define ADXL345_REG_OFSZ        0x20
#define ADXL345_REG_THRESH_ACT  0x24
#define ADXL345_REG_ACT_INACT   0x27
#define ADXL345_REG_BW_RATE     0x2C
#define ADXL345_REG_POWER_CTL   0x2D
#define ADXL345_REG_INT_SOURCE  0x30
#define ADXL345_REG_DATA_FORMAT 0x31
#define ADXL345_REG_DATAX0      0x32
#define ADXL345_REG_FIFO_CTL    0x38
#define ADXL345_REG_FIFO_STATUS 0x39

#endif /* ADXL_REGS_H */
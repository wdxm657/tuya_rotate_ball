/**
 * @file tkl_i2c.c
 * @brief This is tkl_i2c file
 * @version 1.0
 * @date 2021-09-10
 *
 * @copyright Copyright 2021-2023 Tuya Inc. All Rights Reserved.
 *
 */

#include "string.h"
#include "i2c.h"
#include "board.h"

#include "tkl_i2c.h"
#include "tkl_memory.h"

/***********************************************************************
 ********************* constant ( macro and enum ) *********************
 **********************************************************************/
STATIC CONST I2C_GPIO_GroupTypeDef sg_tkl_i2c_gpio_group_index[] = {I2C_GPIO_GROUP_C2C3, I2C_GPIO_GROUP_C0C1, I2C_GPIO_GROUP_B6D7, I2C_GPIO_GROUP_A3A4};

/***********************************************************************
 ********************* struct ******************************************
 **********************************************************************/


/***********************************************************************
 ********************* variable ****************************************
 **********************************************************************/


/***********************************************************************
 ********************* function ****************************************
 **********************************************************************/
STATIC UINT32_T tkl_i2c_freq_transform(TUYA_IIC_SPEED_E speed)
{
    UINT32_T freq = 0xFFFFFFFF;

    switch (speed) {
        case TUYA_IIC_BUS_SPEED_100K: {
            freq = 100000;
        } break;
        case TUYA_IIC_BUS_SPEED_400K: {
            freq = 400000;
        } break;
        case TUYA_IIC_BUS_SPEED_1M: {
            freq = 1000000;
        } break;
        case TUYA_IIC_BUS_SPEED_3_4M: {
            freq = 3400000;
        } break;
        default: {
        } break;
    }

    return freq;
}

STATIC OPERATE_RET tkl_i2c_send(UINT8_T dev_addr, UINT8_T reg_addr, UINT8_T *buf, UINT8_T size)
{
    OPERATE_RET ret = OPRT_OK;

    // address
    reg_i2c_id  = dev_addr | 0x00;
    reg_i2c_adr = reg_addr;
    // lanuch start /id/04    start
    reg_i2c_ctrl = (FLD_I2C_CMD_ID | FLD_I2C_CMD_ADDR | FLD_I2C_CMD_START);
    while (reg_i2c_status & FLD_I2C_CMD_BUSY);
    if (reg_i2c_status & FLD_I2C_NAK) {
        ret = OPRT_TIMEOUT;
    }

    if (ret == OPRT_OK) {
        if (buf) {
            // write data
            UINT8_T index = 0;
            do {
                reg_i2c_di = buf[index];
                // launch data read cycle
                reg_i2c_ctrl = FLD_I2C_CMD_DI;
                while (reg_i2c_status & FLD_I2C_CMD_BUSY);
                if (reg_i2c_status & FLD_I2C_NAK) {
                    ret = OPRT_TIMEOUT;
                }

                index++;
            } while (index < size);
        }
    }

    // stop
    reg_i2c_ctrl = FLD_I2C_CMD_STOP;
    while (reg_i2c_status & FLD_I2C_CMD_BUSY);

    return ret;
}

STATIC OPERATE_RET tkl_i2c_read(UINT8_T dev_addr, UINT32_T reg_addr, UINT8_T *buf, UINT8_T size)
{
    OPERATE_RET ret = OPRT_OK;

    // start + dev_addr(write) + reg_addr
    reg_i2c_id   = dev_addr | 0x00;
    reg_i2c_adr  = reg_addr;
    reg_i2c_ctrl = (FLD_I2C_CMD_ID | FLD_I2C_CMD_ADDR | FLD_I2C_CMD_START);
    while (reg_i2c_status & FLD_I2C_CMD_BUSY);
    if (reg_i2c_status & FLD_I2C_NAK) {
        ret = OPRT_TIMEOUT;
    }

    // restart + dev_addr(read)
    reg_i2c_id   = dev_addr | 0x01;
    reg_i2c_ctrl = (FLD_I2C_CMD_ID | FLD_I2C_CMD_START);
    while (reg_i2c_status & FLD_I2C_CMD_BUSY);
    if (reg_i2c_status & FLD_I2C_NAK) {
        ret = OPRT_TIMEOUT;
    }

    if (ret == OPRT_OK) {
        // read data
        UINT8_T index = 0;
        size--;

        // if not the last byte master read slave, master wACK to slave
        while (size) {
            reg_i2c_ctrl = (FLD_I2C_CMD_DI | FLD_I2C_CMD_READ_ID);
            while (reg_i2c_status & FLD_I2C_CMD_BUSY);
            buf[index] = reg_i2c_di;
            index++;
            size--;
        }

        // when the last byte, master will ACK to slave
        reg_i2c_ctrl = (FLD_I2C_CMD_DI | FLD_I2C_CMD_READ_ID | FLD_I2C_CMD_ACK);
        while (reg_i2c_status & FLD_I2C_CMD_BUSY);
        buf[index] = reg_i2c_di;
    }

    // stop
    reg_i2c_ctrl = FLD_I2C_CMD_STOP;
    while (reg_i2c_status & FLD_I2C_CMD_BUSY);

    return ret;
}

OPERATE_RET tkl_i2c_init(TUYA_I2C_NUM_E port, CONST TUYA_IIC_BASE_CFG_T *cfg)
{
    if (port > TUYA_I2C_NUM_3) {
        return OPRT_NOT_SUPPORTED;
    }

    if (cfg == NULL) {
        return OPRT_INVALID_PARM;
    }

    UINT32_T baudrate = tkl_i2c_freq_transform(cfg->speed);
    if (baudrate == 0xFFFFFFFF) {
        return OPRT_NOT_SUPPORTED;
    }

    // Note:PCx will toggle (pulse width is around 60ns) when wakeup from deepsleep
    i2c_gpio_set(sg_tkl_i2c_gpio_group_index[port]);
    i2c_master_init(0, (UINT8_T)(CLOCK_SYS_CLOCK_HZ / (4 * baudrate)));

    return OPRT_OK;
}

OPERATE_RET tkl_i2c_deinit(TUYA_I2C_NUM_E port)
{
    switch (port) {
        case TUYA_I2C_NUM_0: {
            gpio_set_func(GPIO_PC2, AS_GPIO);
            gpio_set_func(GPIO_PC3, AS_GPIO);
        } break;
        case TUYA_I2C_NUM_1: {
            gpio_set_func(GPIO_PC0, AS_GPIO);
            gpio_set_func(GPIO_PC1, AS_GPIO);
        }
        case TUYA_I2C_NUM_2: {
            gpio_set_func(GPIO_PB6, AS_GPIO);
            gpio_set_func(GPIO_PD7, AS_GPIO);
        } break;
        case TUYA_I2C_NUM_3: {
            gpio_set_func(GPIO_PA3, AS_GPIO);
            gpio_set_func(GPIO_PA4, AS_GPIO);
        } break;
        default: {
            return OPRT_NOT_SUPPORTED;
        } break;
    }

    reset_i2c_moudle();

    return OPRT_OK;
}

OPERATE_RET tkl_i2c_irq_init(TUYA_I2C_NUM_E port, CONST TUYA_I2C_IRQ_CB cb)
{
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tkl_i2c_irq_enable(TUYA_I2C_NUM_E port)
{
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tkl_i2c_irq_disable(TUYA_I2C_NUM_E port)
{
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tkl_i2c_master_send(TUYA_I2C_NUM_E port, UINT16_T dev_addr, CONST VOID_T *data, UINT32_T size, BOOL_T xfer_pending)
{
    OPERATE_RET ret = OPRT_OK;

    if (port > TUYA_I2C_NUM_3) {
        return OPRT_NOT_SUPPORTED;
    }

    if (data == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (size) {
        UINT8_T *i2c_data = (UINT8_T *)data;
        UINT32_T reg_addr = i2c_data[0];

        ret = tkl_i2c_send((UINT8_T)(dev_addr << 1), reg_addr, &i2c_data[1], size - 1);
    } else {
        ret = tkl_i2c_send(0x00, 0x00, NULL, 0);
    }

    return ret;
}

OPERATE_RET tkl_i2c_master_receive(TUYA_I2C_NUM_E port, UINT16_T dev_addr, VOID_T *data, UINT32_T size, BOOL_T xfer_pending)
{
    OPERATE_RET ret = OPRT_OK;

    if (port > TUYA_I2C_NUM_3) {
        return OPRT_NOT_SUPPORTED;
    }

    if (data == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    if (size) {
        UINT8_T *i2c_data = (UINT8_T *)data;
        UINT32_T reg_addr = i2c_data[0];

        ret = tkl_i2c_read((UINT8_T)(dev_addr << 1), reg_addr, &i2c_data[1], size - 1);
    } else {
        ret = tkl_i2c_read(0x00, 0x00, NULL, 0);
    }

    return ret;
}

OPERATE_RET tkl_i2c_set_slave_addr(TUYA_I2C_NUM_E port, UINT16_T dev_addr)
{
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tkl_i2c_slave_send(TUYA_I2C_NUM_E port, CONST VOID_T *data, UINT32_T size)
{
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tkl_i2c_slave_receive(TUYA_I2C_NUM_E port, VOID_T *data, UINT32_T size)
{
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tkl_i2c_get_status(TUYA_I2C_NUM_E port, TUYA_IIC_STATUS_T *status)
{
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tkl_i2c_reset(TUYA_I2C_NUM_E port)
{
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tkl_i2c_ioctl(TUYA_I2C_NUM_E port, UINT32_T cmd, VOID_T *args)
{
    return OPRT_NOT_SUPPORTED;
}

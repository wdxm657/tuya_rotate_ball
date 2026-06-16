/**
 * @file app_charge.h
 * @brief 充电检测模块 — 周期定时器轮询 GPIO 检测充电器插入/拔出
 * @version 1.0
 * @date 2026-06-15
 *
 * @copyright Copyright 2026 Tuya Inc. All Rights Reserved.
 *
 * 硬件连接：
 *   CHARGE_DETECT_PIN — GPIO 输入，高电平=充电器插入（通过电阻分压接 VBUS）
 *   CHARGE_SWITCH     — GPIO 输出，控制充电 MOSFET 使能
 *
 * 检测方式：
 *   主：周期定时器(100ms)轮询 GPIO 电平
 *   辅：电池电压监测（当电压 > 4.0V 时辅助判断，在 battery_report 周期中执行）
 */

#ifndef __APP_CHARGE_H__
#define __APP_CHARGE_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************
 ********************* constant ( macro and enum ) *********************
 **********************************************************************/

/** 电压辅助检测阈值 mV（高于此值视为可能正在充电） */
#define CHARGE_VOLT_THRESHOLD_MV    4000

/** 满电判定电压 mV（稳定在此值以上视为满电） */
#define CHARGE_FULL_VOLT_MV         4150

/***********************************************************************
 ********************* function ****************************************
 **********************************************************************/

/**
 * @brief 初始化充电检测（GPIO 输入 + 周期轮询定时器）
 *   - 设置 CHARGE_DETECT_PIN 为上拉输入
 *   - 创建并启动 100ms 周期定时器轮询电平
 *   - 启动时读取一次引脚状态，避免错过初始状态
 */
VOID_T app_charge_init(VOID_T);

/**
 * @brief 获取当前充电器插入状态（通过 GPIO 读取）
 * @return TRUE=充电器已插入
 */
BOOL_T app_charge_is_detected(VOID_T);

#ifdef __cplusplus
}
#endif

#endif /* __APP_CHARGE_H__ */

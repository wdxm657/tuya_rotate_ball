/**
 * @file tal_ble_group.h
 * @brief This is tal_ble_group file
 * @version 1.0
 * @date 2021-09-10
 *
 * @copyright Copyright 2021-2023 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_BLE_GROUP_H__
#define __TAL_BLE_GROUP_H__

#include "tuya_cloud_types.h"
#include "tuya_error_code.h"
#include "tal_bluetooth.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(TUYA_BLE_FEATURE_ENABLE_GROUP) && (TUYA_BLE_FEATURE_ENABLE_GROUP == 1)

/***********************************************************************
 ********************* constant ( macro and enum ) *********************
 **********************************************************************/


/***********************************************************************
 ********************* struct ******************************************
 **********************************************************************/


/***********************************************************************
 ********************* variable ****************************************
 **********************************************************************/


/***********************************************************************
 ********************* function ****************************************
 **********************************************************************/

/**
 * @brief tal_ble_group_init
 *
 * @param[in] ttl: ttl
 * @param[in] tx_num: tx_num
 * @param[in] tx_interval: tx_interval
 * @param[in] tx_interval_rand_range: tx_interval_rand_range
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
uint32_t tal_ble_group_init(uint8_t ttl, uint16_t tx_num, uint16_t tx_interval, uint16_t tx_interval_rand_range);

/**
 * @brief tal_ble_group_dp_report
 *
 * @param[in] data: data
 * @param[in] data_len: data_len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
uint32_t tal_ble_group_dp_report(uint8_t* data, uint16_t data_len);

/**
 * @brief tal_ble_group_remoter_dp_send
 *
 * @param[in] group_idx: group_idx
 * @param[in] data: data
 * @param[in] data_len: data_len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
uint32_t tal_ble_group_remoter_dp_send(uint16_t group_idx, uint8_t* data, uint16_t data_len);

/**
 * @brief tal_ble_group_rx_adv
 *
 * @param[in] adv_report: adv_report
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
uint32_t tal_ble_group_rx_adv(TAL_BLE_ADV_REPORT_T* adv_report);

/**
 * @brief tal_ble_group_proxy_rx
 *
 * @param[in] buf: buf
 * @param[in] size: size
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
uint32_t tal_ble_group_proxy_rx(uint8_t* buf, uint32_t size);

/**
 * @brief tal_ble_group_disconn_handler
 *
 * @param[in] param: none
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
uint32_t tal_ble_group_disconn_handler(void);

/**
 * @brief tal_ble_group_unbond_handler
 *
 * @param[in] param: none
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
uint32_t tal_ble_group_unbond_handler(void);

/**
 * @brief tal_ble_group_tx_ing
 *
 * @param[in] param: none
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
uint8_t tal_ble_group_tx_ing(void);

#endif // TUYA_BLE_FEATURE_ENABLE_GROUP


#ifdef __cplusplus
}
#endif

#endif /* __TAL_BLE_GROUP_H__ */


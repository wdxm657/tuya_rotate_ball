/**
 * @file tbs_crypto_micro_ecc.h
 * @brief This is tbs_crypto_micro_ecc file
 * @version 1.0
 * @date 2021-09-10
 *
 * @copyright Copyright 2021-2023 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TBS_CRYPTO_MICRO_ECC_H__
#define __TBS_CRYPTO_MICRO_ECC_H__

#include "tuya_cloud_types.h"
#include "tuya_error_code.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************
 ********************* constant ( macro and enum ) *********************
 **********************************************************************/
#define ECC_PUBLIC_KEY_LEN      ( 64 )
#define ECC_PRIVATE_KEY_LEN     ( 32 )

typedef enum {
   ECC_ELLIPTIC_CURVE_SECP160R1 = 0,
   ECC_ELLIPTIC_CURVE_SECP192R1,
   ECC_ELLIPTIC_CURVE_SECP224R1,
   ECC_ELLIPTIC_CURVE_SECP256R1,
   ECC_ELLIPTIC_CURVE_SECP256K1,
} ECC_ELLIPTIC_CURVE_TYPE_T;
    
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
 * @brief tbs_crypto_ecc_init.
 *
 * @param[in] param: curve_type
 *
 * @return NONE
 */
OPERATE_RET tbs_crypto_ecc_init(ECC_ELLIPTIC_CURVE_TYPE_T curve_type);

/**
 * @brief Create a public/private key pair.
 *
 * @param[out] public_key: Will be filled in with the public key.
 * @param[out] private_key: Will be filled in with the private key.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tbs_crypto_ecc_keypair_gen(UINT8_T *public_key, UINT8_T *private_key);

/**
 * @brief Compute a shared secret given your secret key and someone else's public key.
 *
 * @param[in] public_key: The public key of the remote party.
 * @param[in] private_key: Your private key.
 * @param[out] secret_key: secret, Will be filled in with the shared secret value.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tbs_crypto_ecc_compute_shared_secret(UINT8_T *public_key, UINT8_T *private_key, UINT8_T *secret_key);

/**
 * @brief Generate an ECDSA signature for a given hash value. Usage: Compute a
 *        hash of the data you wish to sign (SHA-2 is recommended) and pass it
 *        in to this function along with your private key.
 *
 * @param[in] private_key: private key.
 * @param[in] p_hash: The hash of the message to sign.
 * @param[in] hash_size: The size of message_hash in bytes.
 * @param[out] p_sig: signature, Will be filled in with the signature value.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tbs_crypto_ecc_sign(CONST UINT8_T *private_key, CONST UINT8_T *p_hash, UINT32_T hash_size, UINT8_T *p_sig);

/**
 * @brief Verify an ECDSA signature. Usage: Compute the hash of the signed data
 *        using the same hash as the signer and pass it to this function along
 *        with the signer's public key and the signature values (r and s).
 *
 * @param[in] public_key: The signer's public key.
 * @param[in] p_hash: The hash of the signed data.
 * @param[in] hash_size: The size of message_hash in bytes.
 * @param[in] p_sig: The signature value.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tbs_crypto_ecc_verify(CONST UINT8_T *public_key, CONST UINT8_T *p_hash, UINT32_T hash_size, UINT8_T *p_sig);

/**
 * @brief Compress a public key.
 *
 * @param[in] public_key: The public key to compress.
 * @param[out] compressed: Will be filled in with the compressed public key. Must be at least
                 (curve size + 1) bytes long; for example, if the curve is secp256r1,
                 compressed must be 33 bytes long.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tbs_crypto_ecc_compress_public_key(CONST UINT8_T *public_key, UINT8_T *compressed);

/**
 * @brief Decompress a compressed public key.
 *
 * @param[in] compressed: The compressed public key.
 * @param[out] public_key: Will be filled in with the decompressed public key.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tbs_crypto_ecc_decompress_public_key(CONST UINT8_T *compressed, UINT8_T *public_key);

/**
 * @brief Compute the corresponding public key for a private key.
 *
 * @param[in] private_key: The private key to compute the public key for
 * @param[out] public_key: Will be filled in with the corresponding public key
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tbs_crypto_ecc_compute_public_key(CONST UINT8_T *private_key, UINT8_T *public_key);

/**
 * @brief tbs_crypto_ecc_test_example.
 *
 * @param[in] param1: none
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tbs_crypto_ecc_test_example(VOID_T);


#ifdef __cplusplus
}
#endif

#endif /* __TBS_CRYPTO_MICRO_ECC_H__ */


/*
 * ========================================
 * HAL CONFIGURATION FOR STM32_CAN
 * ========================================
 * 
 * This file enables the CAN peripheral in the STM32 HAL.
 * Required for STM32_CAN library to compile and function.
 * 
 * The STM32_CAN library requires HAL_CAN_MODULE_ENABLED to be defined
 * for the CAN peripheral to be available in the HAL layer.
 */

#ifndef HAL_CONF_EXTRA_H
#define HAL_CONF_EXTRA_H

// Enable CAN peripheral in STM32 HAL
#define HAL_CAN_MODULE_ENABLED

#endif // HAL_CONF_EXTRA_H

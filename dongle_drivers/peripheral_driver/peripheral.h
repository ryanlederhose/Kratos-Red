/*
**************************************************************************************************************
* @file     Kratos-Red/dongle_drivers/peripheral_driver/peripheral.c
* @author   Ryan Lederhose - 45836705
* @date     May 2023
* @brief    nrf52840 Drivers - Peripheral BL
**************************************************************************************************************
*/

#ifndef PERIPHERAL_DRIVER_H
#define PERIPHERAL_DRIVER_H

#include <zephyr/shell/shell.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <stddef.h>
#include <zephyr/kernel.h>
#include <string.h>
#include <zephyr/toolchain.h>
#include <zephyr/logging/log.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <zephyr/logging/log_msg.h>
#include <zephyr/logging/log_instance.h>
#include <zephyr/logging/log_core.h>
#include <zephyr/data/json.h>
#include <usb/usb_device.h>
#include <zephyr/types.h>
#include <errno.h>
#include <zephyr/zephyr.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/sys/byteorder.h>
#include <sys/slist.h>
#include <kernel.h>

#define SHELL_THREAD_PRIORITY 2
#define SHELL_THREAD_STACK 500
#define BT_THREAD_PRIORITY 1
#define BT_THREAD_STACK 500
#define UART1_NODELABEL DT_CHOSEN(zephyr_uart2)
#define MSG_SIZE sizeof (hciPacket_t)

#define UUID_BUFFER_SIZE 16

#define HCI_BSU_UUID \
    BT_UUID_DECLARE_128(0xAA, 0x43, 0x54, 0xDD, 0xFD, 0x4E, 0x89, 0xBE, 0x32, 0x22, 0xFF, 0x13, 0x3F, 0x2A, 0x29, 0x95)

#define HCI_AHU_UUID \
    BT_UUID_DECLARE_128(0xDD, 0x43, 0x54, 0xDD, 0xFD, 0x4E, 0x89, 0xBE, 0x32, 0x22, 0xFF, 0x13, 0x3F, 0x2A, 0x29, 0x95)

#define HCI_CHAR_BSU_UUID \
    BT_UUID_DECLARE_128(0xBB, 0x43, 0x54, 0xDD, 0xFD, 0x4E, 0x89, 0xBE, 0x32, 0x22, 0xFF, 0x13, 0x3F, 0x2A, 0x29, 0x95)

#define HCI_CHAR_AHU_UUID \
    BT_UUID_DECLARE_128(0xCC, 0x43, 0x54, 0xDD, 0xFD, 0x4E, 0x89, 0xBE, 0x32, 0x22, 0xFF, 0x13, 0x3F, 0x2A, 0x29, 0x95)

#define SIZE_STRUCT (sizeof (hciPacket_t) - 2)

//Types of commands
#define GESTURE_CMD 1
#define RESET_CMD 2
#define CLEAR_CMD 3
#define START_UP_CMD 4

//Types of gestures
#define GESTURE_RIGHT 1
#define GESTURE_LEFT 2
#define GESTURE_UP 3
#define GESTURE_DOWN 4
#define GESTURE_MOVE_RIGHT 5
#define GESTURE_MOVE_LEFT 6
#define GESTURE_MOVE_UP 7
#define GESTURE_MOVE_DOWN 8

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL,
	  0xAA, 0x43, 0x54, 0xDD, 0xFD, 0x4E, 0x89, 0xBE, 
	  0x32, 0x22, 0xFF, 0x13, 0x3F, 0x2A, 0x29, 0x95),
};

typedef struct __attribute__((packed))_hciPacket {
    uint8_t preamble;
    uint8_t command;
    uint8_t data;
} hciPacket_t;

/**
 * @brief function to send a hci data packet over BLE
 * @param handle handle of gatt characteristic
 * @param pack data packet to send over BLE
 */
static void gatt_write(uint16_t handle, hciPacket_t *pack);

#endif
/*
**************************************************************************************************************
* @file     Kratos-Red/dongle_drivers/dongle.c
* @author   Ryan Lederhose - 45836705
* @date     May 2023
* @brief    nrf52840 Drivers
**************************************************************************************************************
*/

#ifndef BSU_DRIVER_H
#define BSU_DRIVER_H

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

#define BSU_SHELL_THREAD_PRIORITY 2
#define BSU_SHELL_THREAD_STACK 500
#define BSU_BT_THREAD_PRIORITY 1
#define BSU_BT_THREAD_STACK 500
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

#define HCI_MNODE_UUID \
    BT_UUID_DECLARE_128(0xFA, 0x43, 0x54, 0xDD, 0xFD, 0x4E, 0x89, 0xBE, 0x32, 0x22, 0xFF, 0x13, 0x3F, 0x2A, 0x29, 0x95)

#define HCI_CHAR_MNODE_UUID \
    BT_UUID_DECLARE_128(0xFB, 0x43, 0x54, 0xDD, 0xFD, 0x4E, 0x89, 0xBE, 0x32, 0x22, 0xFF, 0x13, 0x3F, 0x2A, 0x29, 0x95)

#define ZERO_ID 0x00
#define TEMPERATURE_ID 0x01
#define PRESSURE_ID 0x02
#define X_ACC_ID 0x03
#define Y_ACC_ID 0x04
#define Z_ACC_ID 0x05
#define X_GYR_ID 0x06
#define Y_GYR_ID 0x07
#define Z_GYR_ID 0x08
#define X_MAG_ID 0x09
#define Y_MAG_ID 10
#define Z_MAG_ID 11
#define ULTRASONIC_ID 12
#define ORIENTATION_ID 13
#define ALTITUDE 14
#define LED0_ID 15
#define LED1_ID 16
#define PB_ID 17
#define SAMPLE_ID 18
#define ALL_ID 19
#define CONT_ID 20

#define SIZE_STRUCT (sizeof (hciPacket_t) - 2)

typedef struct __attribute__((packed))_hciPacket {
    uint8_t preamble;
    uint8_t msgLen;
    uint8_t msgRw;
    uint8_t devID;
    float msgData;
    uint8_t newLine;
} hciPacket_t;

typedef struct __attribute__((packed))_rssiPacket {
    uint8_t preamble;
    uint8_t msgLen;
    int8_t rssi[8];
    uint8_t node[8];
    uint8_t newLine;
} rssiPacket_t;

typedef struct __attribute__((packed))_bleRssi {
    uint8_t preamble;
    char addr[BT_ADDR_LE_STR_LEN];
    int8_t rssi;
    uint8_t node;
} bleRssi_t;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL,
	  0xAA, 0x43, 0x54, 0xDD, 0xFD, 0x4E, 0x89, 0xBE, 
	  0x32, 0x22, 0xFF, 0x13, 0x3F, 0x2A, 0x29, 0x95),
};

/**
 * @brief function to send a hci data packet over BLE
 * @param handle handle of gatt characteristic
 * @param pack data packet to send over BLE
 */
static void gatt_write(uint16_t handle, hciPacket_t *pack);

/**
 * @brief start sending BLE advertising packets
 */
static void ble_start_advertising(void);


/* Node ids */
#define NODE_A_ID 1
#define NODE_B_ID 2
#define NODE_C_ID 3
#define NODE_D_ID 4
#define NODE_E_ID 5
#define NODE_F_ID 6
#define NODE_G_ID 7
#define NODE_H_ID 8
#define NODE_I_ID 9
#define NODE_J_ID 10
#define NODE_K_ID 11
#define NODE_L_ID 12

/* Active nodes postions */
#define NODE_POSITION_ONE 		0
#define NODE_POSITION_TWO 		1
#define NODE_POSITION_THREE		2
#define NODE_POSITION_FOUR		3
#define NODE_POSITION_FIVE		4
#define NODE_POSITION_SIX		5
#define NODE_POSITION_SEVEN		6
#define NODE_POSITION_EIGHT		7

#endif
/*
**************************************************************************************************************
* @file     Kratos-Red/dongle_drivers/peripheral_driver/peripheral.c
* @author   Ryan Lederhose - 45836705
* @date     May 2023
* @brief    nrf52840 Drivers - Peripheral BL
**************************************************************************************************************
*/

/* Local Library */
#include "peripheral.h"

//Define log register
LOG_MODULE_REGISTER(log_module, 4);

//Define the BLE command message queue
K_MSGQ_DEFINE(command_msgq, sizeof (hciPacket_t), 20, 4);

static struct bt_conn *default_conn;
static struct bt_uuid *hci_uuid_ptr = HCI_BSU_UUID;
uint8_t chrcFlag = 0x00;

static uint16_t hci_write_handle;

/**
 * @brief process incoming commands and print serially for rpi
 * @param pack incoming data packet
*/
static void json_process(hciPacket_t pack) {
    printk("{\r\n");
    printk("  <Command>: %d\r\n", pack.command);
    printk("  <Data>: %d\r\n", pack.data);
    printk("}\r\n");
}
/**
 * @brief start sending BLE advertising packets
 */
static void ble_start_advertising(void) {

    int err;
    err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);

    if (err) {
		LOG_ERR("Advertising failed to start (err %d)\n", err);
		return;
	}

	LOG_INF("Advertising successfully started\n");
}

/**
 * @brief Callback function for a gatt discovery of a BLE connection
 * @param conn connection handle 
 * @param attr attribute data for connection
 * @param params parameters of gatt discovery
 * @return uint8_t
 */
static uint8_t gatt_discover_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			     struct bt_gatt_discover_params *params) {
	int err;

    //Check if attribute is null
	if (attr == NULL) {
		if (hci_write_handle == 0) {
            LOG_ERR("Did not discover AHU/MNODE GATT");
		}
		memset(params, 0, sizeof(*params));
		return BT_GATT_ITER_STOP;
	}

    LOG_DBG("Attribute handle %u", attr->handle);

    //Check for primary service
	if (params->type == BT_GATT_DISCOVER_PRIMARY && bt_uuid_cmp(params->uuid, HCI_BSU_UUID) == 0) {
		LOG_DBG("Found AHU primary service");

		params->uuid = NULL;
		params->start_handle = attr->handle + 1;
		params->type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, params);   //Discover GATT characteristics
		if (err != 0) {
            LOG_ERR("Discovery of GATT Characteristics failed (err %d)", err);
		}

		return BT_GATT_ITER_STOP;
	} else if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC) {
		struct bt_gatt_chrc *chrcGatt = (struct bt_gatt_chrc *) attr->user_data;
        
        if (bt_uuid_cmp(chrcGatt->uuid, HCI_CHAR_BSU_UUID) == 0) {
            LOG_DBG("Found AHU Characteristic");

			hci_write_handle = chrcGatt->value_handle;  //Save GATT characteristic handle
            chrcFlag = 0x01;
        }
	}

	return BT_GATT_ITER_CONTINUE;
}


/**
 * @brief Discover the GATT services and characteristics of a BLE connection
 */
static void gatt_discover(void) {
	static struct bt_gatt_discover_params gattDiscoverParams;
	int err;

    LOG_DBG("Begin discovering services and characteristics");

    //Set gattDiscoverParams
    gattDiscoverParams.type = BT_GATT_DISCOVER_PRIMARY;
    gattDiscoverParams.uuid = hci_uuid_ptr;
	gattDiscoverParams.func = gatt_discover_cb;
	gattDiscoverParams.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	gattDiscoverParams.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;

	err = bt_gatt_discover(default_conn, &gattDiscoverParams);  //Discover GATT
	if (err != 0) {
        LOG_ERR("Discovery of gatt services and characteristics failed");
        return;
    }

	LOG_DBG("Discovery complete");
}

/**
 * @brief BLE connection callback function
 * @param conn connection handle
 * @param err error code
 */
static void connected(struct bt_conn *conn, uint8_t err) {
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err != 0) {
		LOG_ERR("Failed to connect to %s (%u)", addr, err);

		bt_conn_unref(default_conn);
		default_conn = NULL;

		ble_start_advertising(); 
		return;
	}

	LOG_DBG("Connected to %s", addr);

	default_conn = conn;

	LOG_DBG("Discovering services and characteristics");

	gatt_discover();    //Discover gatt
}

/**
 * @brief BLE disconnected callback function
 * @param conn connection handle
 * @param reason reason for disconnection
 */
static void disconnected(struct bt_conn *conn, uint8_t reason) {
	char addr[BT_ADDR_LE_STR_LEN];

	if (conn != default_conn) {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_ERR("Disconnected: %s (reason 0x%02x)\n", addr, reason);

	bt_conn_unref(default_conn);
	default_conn = NULL;

	ble_start_advertising();   //start advertising again
}

/**
 * @brief gatt write callback function
 * @param conn connection handle]
 * @param err error code
 * @param params gatt parameters
 */
static void hci_write_gatt(struct bt_conn *conn, uint8_t err, struct bt_gatt_write_params *params) {

	if (err != BT_ATT_ERR_SUCCESS) {
		printk("Write failed: 0x%02X\n", err);
    }

	memset(params, 0, sizeof(*params));

    return;
}

/**
 * @brief callback function to read an incoming message over gatt
 * @param conn connection handle
 * @param attr attributes of gatt
 * @param buf incoming dat 
 * @param len data length
 * @param flags set flags
 */
static void hci_read_gatt(struct bt_conn *conn, const struct bt_gatt_attr *attr, 
                const uint8_t *buf, uint16_t len, uint8_t flags) {

    hciPacket_t pack;

    uint8_t *buffer = buf;
	
	//Parse data for message data. Check preamble
	if (buffer[0] == 0xAA) {
		for (int i = 0; i < len; i++) {
			switch (i) {
				case 0:
					pack.preamble = buffer[i];
				case 1:
					pack.command = buffer[i];
				case 2:
					pack.data = buffer[i];
			}
		}
	}     

    k_msgq_put(&command_msgq, &pack, K_NO_WAIT);
    return;
}

/**
 * @brief function to send a hci data packet over BLE
 * @param handle handle of gatt characteristic
 * @param pack data packet to send over BLE
 */
static void gatt_write(uint16_t handle, hciPacket_t *pack) {

	static struct bt_gatt_write_params params;
	int err;
    char *buf = (char *) pack;

    params.data = buf;
    params.length = sizeof(hciPacket_t);
	params.func = hci_write_gatt;
	params.handle = handle;

	err = bt_gatt_write(default_conn, &params);
	if (err != 0) {
		LOG_ERR("Failed write (err 0x%X)", err);
	}
}

/**
 * @brief define a new service for the bsu along with corresponding characteristics
 */
BT_GATT_SERVICE_DEFINE(bsu_ahu_service, BT_GATT_PRIMARY_SERVICE(HCI_AHU_UUID),
	BT_GATT_CHARACTERISTIC(HCI_CHAR_AHU_UUID,
    			    (BT_GATT_CHRC_WRITE_WITHOUT_RESP),
			       (BT_GATT_PERM_WRITE), NULL,
			       hci_read_gatt, NULL),
);

/**
 * @brief define the connection callback functions for BLE
 */
BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

/**
 * @brief initialise the shell along with the necessary commands for the Base Station Unit (BSU)
 */
void dongle_shell_init() {
}

/**
 * @brief Main thread for Base Station Unit's (BSU command) command line interface implementation
 */
void dongle_shell_thread(void) {
    const struct device *dev_bsu_shell;
    uint32_t dtr = 0;
    uint8_t val = 1;

    //Get the device struct for the shell and check if it is ready for use
    dev_bsu_shell = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
    if (!device_is_ready(dev_bsu_shell)) {
        return;
    }

    /* Enable the USB Driver */
    if (usb_enable(NULL)) { 
        return;
    }

    //Retrieve the line control for UART
    while (!val) {
        val = uart_line_ctrl_get(dev_bsu_shell, UART_LINE_CTRL_DTR, &dtr);
        k_msleep(100);
    }

    dongle_shell_init();   //initialise the shell

    hciPacket_t txPacket;

    for (;;) {
        
        //Check if we need to send a message of BLE 
        if (k_msgq_get(&command_msgq, &txPacket, K_MSEC(10)) == 0) {
            json_process(txPacket);
        }

        k_msleep(5);
    }
}

/**
 * @brief Main thread for Base Station Unit's (BSU command) bluetooth interface
 */
void bt_thread(void) {
    int err;

    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)\r\n", err);
        return;
    }

    LOG_DBG("Bluetooth initialised\r\n");

    //Start advertising over BLE
    ble_start_advertising();
}

/* Define ahu_shell and ahu_uart threads */
K_THREAD_DEFINE(shell_thread_tid, SHELL_THREAD_STACK, dongle_shell_thread, 
        NULL, NULL, NULL, SHELL_THREAD_PRIORITY, 0, 0);
K_THREAD_DEFINE(bt_thread_tid, BT_THREAD_STACK, bt_thread, 
        NULL, NULL, NULL, BT_THREAD_PRIORITY, 0, 0);
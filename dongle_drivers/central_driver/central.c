/*
**************************************************************************************************************
* @file     Kratos-Red/dongle_drivers/central_drivers/central.c
* @author   Ryan Lederhose - 45836705
* @date     May 2023
* @brief    nrf52840 Drivers - Central BLE
**************************************************************************************************************
*/

/* Local Library */
#include "central.h"

static void start_scan(void);

//Define log register
LOG_MODULE_REGISTER(log_module, 4);

//Define the BLE command message queue
K_MSGQ_DEFINE(command_msgq, sizeof(hciPacket_t), 20, 4);

static struct bt_conn* default_conn;
static struct bt_uuid* hci_uuid_ptr = HCI_AHU_UUID;
uint8_t chrcFlag = 0x00;

uint16_t hci_uuid[] = { 0xAA, 0x43, 0x54, 0xDD, 0xFD, 0x4E, 0x89, 0xBE, 0x32, 0x22, 0xFF, 0x13, 0x3F, 0x2A, 0x29, 0x95 };

static uint16_t hci_write_handle;

/**
 * @brief start or stop scanning for BLE AHU+SCU
 * @param shell pointer to shell struct
 * @param argc number of arguments
 * @param argv argument string array
 */
static void cmd_scan(const struct shell* shell, size_t argc, char** argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    //Check for invalid arguments
    if ((argc != 2) || (strlen(argv[1]) > 1)) {
        LOG_ERR("Incorrect number of commands");
        return;
    }

    if (argv[1][0] == 'o') {
        start_scan();
    } else if (argv[1][0] == 'f') {
        LOG_INF("Stopping BLE scan");
        bt_le_scan_stop();
    } else {
        LOG_ERR("Incorrect number of commands");
        return;
    }
}

/**
 * @brief send a gesture over BLE to the RPi
 * @param shell pointer to shell struct
 * @param argv argument string array
*/
static void cmd_gesture(const struct shell* shell, size_t argc, char** argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    hciPacket_t pack;
    pack.preamble = 0xAA;
    pack.command = GESTURE_CMD;

    //Check for invalid arguments
    if ((argc != 2) || (strlen(argv[1]) > 1)) {
        LOG_ERR("Incorrect number of commands");
        return;
    }

    if (((int)argv[1][0] - 48) <= 12) {
        pack.data = ((int)argv[1][0] - 48);
        gatt_write(hci_write_handle, &pack);
    }
}

/**
 * @brief send a gesture over BLE to the RPi
 * @param shell pointer to shell struct
 * @param argv argument string array
*/
static void cmd_reset(const struct shell* shell, size_t argc, char** argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    hciPacket_t pack;
    pack.preamble = 0xAA;
    pack.command = RESET_CMD;
    pack.data = 0x00;

    //Check for invalid arguments
    if (argc != 1) {
        LOG_ERR("Incorrect number of commands");
        return;
    }

    gatt_write(hci_write_handle, &pack);
}

/**
 * @brief Parse the incoming user data over BLE to create a connection
 * @param data bluetooth struct data
 * @param user_data user data for bluetooth
 * @return bool status of BLE connection
 */
static bool data_parse_cb(struct bt_data* data, void* user_data) {

    bt_addr_le_t* addr = user_data;
    int count = 0;

    //Check the UUID
    if (data->type == BT_DATA_UUID128_ALL) {
        for (int i = 0; i < data->data_len; i++) {
            if (data->data[i] == hci_uuid[i]) {
                count++;
            }
        }
        if (count == UUID_BUFFER_SIZE) {
            int err;
            err = bt_le_scan_stop(); //Stop scanning
            k_msleep(10);
            if (err) {
                LOG_ERR("Scan Stop Failed\r\n");
                return true;
            }

            //Create BLE connection
            struct bt_le_conn_param* connectionParams = BT_LE_CONN_PARAM_DEFAULT;
            err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, connectionParams, &default_conn);
            if (err) {
                LOG_ERR("Connection failed\r\n");
                start_scan(); //Scan again
            }
            return false;
        }
    }
    return true;
}

/**
 * @brief callback function for when BLE has scanned a device for connection
 * @param addr bluetooth address
 * @param rssi rssi of BLE connection
 * @param type type of connection
 * @param ad bluetooth data struct
 */
static void device_found(const bt_addr_le_t* addr, int8_t rssi, uint8_t type,
                         struct net_buf_simple* ad) {

    char addr_str[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

    //Exit if device is not conectable
    if (type != BT_GAP_ADV_TYPE_ADV_IND && type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
        return;
    }

    bt_data_parse(ad, data_parse_cb, (void*)addr);
}

/**
 * @brief Start an active BLE scan for nearby devices.
 */
static void start_scan(void) {
    int err;

    struct bt_le_scan_param btScanParams = {
        .type = BT_LE_SCAN_TYPE_ACTIVE,
        .options = BT_LE_SCAN_OPT_NONE,
        .interval = BT_GAP_SCAN_FAST_INTERVAL,
        .window = BT_GAP_SCAN_FAST_WINDOW,
    };

    //Start scanning
    err = bt_le_scan_start(&btScanParams, device_found);
    if (err) {
        printk("Scanning failed to start (err %d)\n", err);
        return;
    }

    printk("Scanning successfully started\n");
}

/**
 * @brief Callback function for a gatt discovery of a BLE connection
 * @param conn connection handle 
 * @param attr attribute data for connection
 * @param params parameters of gatt discovery
 * @return uint8_t
 */
static uint8_t gatt_discover_cb(struct bt_conn* conn, const struct bt_gatt_attr* attr,
                                struct bt_gatt_discover_params* params) {
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
    if (params->type == BT_GATT_DISCOVER_PRIMARY && bt_uuid_cmp(params->uuid, HCI_AHU_UUID) == 0) {
        LOG_DBG("Found AHU primary service");

        params->uuid = NULL;
        params->start_handle = attr->handle + 1;
        params->type = BT_GATT_DISCOVER_CHARACTERISTIC;

        err = bt_gatt_discover(conn, params); //Discover GATT characteristics
        if (err != 0) {
            LOG_ERR("Discovery of GATT Characteristics failed (err %d)", err);
        }

        return BT_GATT_ITER_STOP;
    } else if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC) {
        struct bt_gatt_chrc* chrcGatt = (struct bt_gatt_chrc*)attr->user_data;

        if (bt_uuid_cmp(chrcGatt->uuid, HCI_CHAR_AHU_UUID) == 0) {
            LOG_DBG("Found AHU Characteristic");

            hci_write_handle = chrcGatt->value_handle; //Save GATT characteristic handle
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

    err = bt_gatt_discover(default_conn, &gattDiscoverParams); //Discover GATT
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
static void connected(struct bt_conn* conn, uint8_t err) {
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (err != 0) {
        LOG_ERR("Failed to connect to %s (%u)", addr, err);

        bt_conn_unref(default_conn);
        default_conn = NULL;

        start_scan();
        return;
    }

    LOG_DBG("Connected to %s", addr);

    default_conn = conn;

    LOG_DBG("Discovering services and characteristics");

    gatt_discover(); //Discover gatt
}

/**
 * @brief BLE disconnected callback function
 * @param conn connection handle
 * @param reason reason for disconnection
 */
static void disconnected(struct bt_conn* conn, uint8_t reason) {
    char addr[BT_ADDR_LE_STR_LEN];

    if (conn != default_conn) {
        return;
    }

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_ERR("Disconnected: %s (reason 0x%02x)\n", addr, reason);

    bt_conn_unref(default_conn);
    default_conn = NULL;

    start_scan(); //start scanning again
}

/**
 * @brief gatt write callback function
 * @param conn connection handle]
 * @param err error code
 * @param params gatt parameters
 */
static void hci_write_gatt(struct bt_conn* conn, uint8_t err, struct bt_gatt_write_params* params) {

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
static void hci_read_gatt(struct bt_conn* conn, const struct bt_gatt_attr* attr,
                          const uint8_t* buf, uint16_t len, uint8_t flags) {
    return;
}

/**
 * @brief function to send a hci data packet over BLE
 * @param handle handle of gatt characteristic
 * @param pack data packet to send over BLE
 */
static void gatt_write(uint16_t handle, hciPacket_t* pack) {

    static struct bt_gatt_write_params params;
    int err;
    char* buf = (char*)pack;

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
BT_GATT_SERVICE_DEFINE(bsu_ahu_service, BT_GATT_PRIMARY_SERVICE(HCI_BSU_UUID),
                       BT_GATT_CHARACTERISTIC(HCI_CHAR_BSU_UUID,
                                              (BT_GATT_CHRC_WRITE_WITHOUT_RESP),
                                              (BT_GATT_PERM_WRITE), NULL,
                                              hci_read_gatt, NULL), );

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

    //Create and initialise the 'scan' commands
    SHELL_CMD_REGISTER(scan, NULL, "Start or stop scanning for the AHU+SCU device", cmd_scan);

    //Create and initialise the 'gesture' commands
    SHELL_CMD_REGISTER(gesture, NULL, "Send a gesture to the rpi", cmd_gesture);

    //Create and initialise the 'reset' commands
    SHELL_CMD_REGISTER(reset, NULL, "Reset the led matrix", cmd_reset);
}

/**
 * @brief Main thread for Base Station Unit's (BSU command) command line interface implementation
 */
void dongle_shell_thread(void) {
    const struct device* dev_bsu_shell;
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

    dongle_shell_init(); //initialise the shell

    hciPacket_t txPacket;

    for (;;) {

        //Check if we need to send a message of BLE
        if (k_msgq_get(&command_msgq, &txPacket, K_MSEC(10)) == 0) {
            if (chrcFlag == 0x01) {
                gatt_write(hci_write_handle, &txPacket);
            }
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
}

/* Define ahu_shell and ahu_uart threads */
K_THREAD_DEFINE(shell_thread_tid, SHELL_THREAD_STACK, dongle_shell_thread,
                NULL, NULL, NULL, SHELL_THREAD_PRIORITY, 0, 0);
K_THREAD_DEFINE(bt_thread_tid, BT_THREAD_STACK, bt_thread,
                NULL, NULL, NULL, BT_THREAD_PRIORITY, 0, 0);
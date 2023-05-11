/*
**************************************************************************************************************
* @file     repo/oslib/bsu_drivers/bsu_hci/bsu_hci.c
* @author   Ryan Lederhose - 45836705
* @date     17/03/2023
* @brief    BSU HCI Driver
**************************************************************************************************************
*/

/* Local Library */
#include "bsu_hci.h"

static void start_scan(void);

//Define log register
LOG_MODULE_REGISTER(bsu_log_module, 4);

//Define the BLE command message queue
K_MSGQ_DEFINE(ble_cmd_msgq, sizeof (hciPacket_t), 20, 4);

static struct bt_conn *default_conn;
static struct bt_uuid *hci_uuid_ptr = HCI_AHU_UUID;
static struct bt_uuid *mnode_uuid_ptr = HCI_MNODE_UUID;
uint8_t chrcFlag = 0x00;

static uint8_t ld1State = 0x00;
static uint8_t ld0State = 0x00;
static float sampleRate = 0x00;
static uint8_t jsonFlag = 0x00;
static uint8_t jsonIndex = 0x00;
static uint8_t orientationIndex = 0x00;
static uint8_t hciFlag = 0x00;

uint16_t hci_uuid[] = {0xAA, 0x43, 0x54, 0xDD, 0xFD, 0x4E, 0x89, 0xBE, 0x32, 0x22, 0xFF, 0x13, 0x3F, 0x2A, 0x29, 0x95};
uint16_t mnode_uuid[] = {0xFA, 0x43, 0x54, 0xDD, 0xFD, 0x4E, 0x89, 0xBE, 0x32, 0x22, 0xFF, 0x13, 0x3F, 0x2A, 0x29, 0x95};

static uint16_t hci_write_handle;

sys_slist_t node_list;

int active_nodes[12] = {
	NODE_A_ID, 
	NODE_B_ID, 
	NODE_C_ID, 
	NODE_D_ID, 
	NODE_E_ID, 
	NODE_F_ID, 
	NODE_G_ID, 
	NODE_H_ID
};

struct data_node {
	sys_snode_t node;
	int* data;
	int* x;
	int* y;
};

typedef struct Node_Struct {
	char* name;
	char* address;
	int major_num;
	int minor_num;
} Node_Struct;


/* Stores the information of all 12 nodes */
Node_Struct node_all[12] = {
	{"4011-A", "F5:75:FE:85:34:67", 2753, 32998},
	{"4011-B", "E5:73:87:06:1E:86", 32975, 20959},
	{"4011-C", "CA:99:9E:FD:98:B1", 26679, 40363},
	{"4011-D", "CB:1B:89:82:FF:FE", 41747, 38800},
	{"4011-E", "D4:D2:A0:A4:5C:AC", 30679, 51963},
	{"4011-F", "C1:13:27:E9:B7:7C", 6195, 18394},
	{"4011-G", "F1:04:48:06:39:A0", 30525, 30544},
	{"4011-H", "CA:0C:E0:DB:CE:60", 57395, 28931},
	{"4011-I", "D4:7F:D4:7C:20:13", 60345, 49995},
	{"4011-J", "F7:0B:21:F1:C8:E1", 12249, 30916},
	{"4011-K", "FD:E0:8D:FA:3E:4A", 36748, 11457},
	{"4011-L", "EE:32:F7:28:FA:AC", 27564, 27589}
};

sys_snode_t* find_node(int node_id) {
	if (node_id < 1 || node_id > 12) {
		return 0;
	}
	sys_snode_t* node = sys_slist_peek_head(&node_list);
	while (true) {
		int node_number = *((struct data_node *)node)->data;
		if (node_id == node_number) {
			return node;
		}
		if (node != sys_slist_peek_tail(&node_list)) {
			node = sys_slist_peek_next(node);
		} else {
			return 0;
		}
	}
	return 0;
}


int get_node_x(int node_id) {
	sys_snode_t* node = find_node(node_id);
	if (node == 0) {
		return -1;
	}
	return *((struct data_node*)node)->x;
}

int get_node_y(int node_id) {
	sys_snode_t* node = find_node(node_id);
	if (node == 0) {
		return -1;
	}
	return *((struct data_node*)node)->y;
}

void update_active_nodes(void) {
	for (int i = 1; i <= 12; i++) {
		switch(get_node_x(i)) {
			case 0:
				switch(get_node_y(i)) {
					case 0:
						active_nodes[NODE_POSITION_ONE] = i;
						break;
					case 2:
						active_nodes[NODE_POSITION_EIGHT] = i;
						break;
					case 4:
						active_nodes[NODE_POSITION_SEVEN] = i;
						break;
					default:
						printk("Error at node update\n");
				}
				break;
			case 2:
				switch(get_node_y(i)) {
					case 0:
						active_nodes[NODE_POSITION_TWO] = i;
						break;
					case 4:
						active_nodes[NODE_POSITION_SIX] = i;
						break;
					default:
						printk("Error at node update\n");
				}
				break;
			case 4:
				switch(get_node_y(i)) {
					case 0:
						active_nodes[NODE_POSITION_THREE] = i;
						break;
					case 2:
						active_nodes[NODE_POSITION_FOUR] = i;
						break;
					case 4:
						active_nodes[NODE_POSITION_FIVE] = i;
						break;
					default:
						printk("Error at node update\n");
				}
		}
	}
}

struct data_node* data_node_init(void) {
	struct data_node* node;
	node = (struct data_node*)k_malloc(sizeof(struct data_node));
	node->data = (int*)k_malloc(sizeof(int));
	node->x = (int*)k_malloc(sizeof(int));
	node->y = (int*)k_malloc(sizeof(int));
	return node;
}

void data_node_free(sys_snode_t* node) {
	k_free(((struct data_node*)node)->data);
	k_free(((struct data_node*)node)->x);
	k_free(((struct data_node*)node)->y);
	k_free(node);
}


void add_node(int data, int x, int y) {
	struct data_node* data_node = data_node_init();
	*data_node->data = data;
	*data_node->x = x;
	*data_node->y = y;
	sys_slist_append(&node_list, &data_node->node);
	update_active_nodes();
}

void init_nodes(void) {
    sys_slist_init(&node_list); //init nodelist

    add_node(NODE_A_ID, 0, 0);
	add_node(NODE_B_ID, 2, 0);
	add_node(NODE_C_ID, 4, 0);
	add_node(NODE_D_ID, 4, 2);
	add_node(NODE_E_ID, 4, 4);
	add_node(NODE_F_ID, 2, 4);
	add_node(NODE_G_ID, 0, 4);
	add_node(NODE_H_ID, 0, 2);


}

sys_snode_t* get_next_node(sys_snode_t* target) {
	if (target == sys_slist_peek_tail(&node_list)) {
		return sys_slist_peek_head(&node_list);
	}
	return sys_slist_peek_next(target);
}

sys_snode_t* get_last_node(sys_snode_t* target) {
	sys_snode_t* head_node = sys_slist_peek_head(&node_list);
	sys_snode_t* tail_node = sys_slist_peek_tail(&node_list);
	sys_snode_t* last_node = NULL;
	if (head_node == tail_node) {
		return head_node;
	}
	if (target == head_node) {
		return tail_node;
	} 
	while (true) {
		last_node = head_node;
		head_node = sys_slist_peek_next(head_node);
		if (target == head_node) {
			return last_node;
		}
		if (head_node == sys_slist_peek_tail(&node_list)) {
			return NULL;
		}
	}
}

void print_node_info(int node_id) {
	sys_snode_t* node = find_node(node_id);
	int x = *((struct data_node*)node)->x;
	int y = *((struct data_node*)node)->y;
	printk("Node ID: %d has the following properties:\n", node_id);
	printk("Node name: %s\n", node_all[node_id-1].name);
	printk("Node MAC Address: %s\n", node_all[node_id-1].address);
	printk("Node major number: %d\n", node_all[node_id-1].major_num);
	printk("Node minor number: %d\n", node_all[node_id-1].minor_num);
	printk("Node X coordinate: %d\n", x);
	printk("Node Y coordinate: %d\n", y);
}

void print_node_neighbours(int node_id) {
	sys_snode_t* node = find_node(node_id);
	sys_snode_t* next_node = get_next_node(node);
	sys_snode_t* last_node = get_last_node(node);
	int next_id = *((struct data_node*)next_node)->data;
	int last_id = *((struct data_node*)last_node)->data;
	printk("Node left neighbour: %s\n", node_all[last_id-1].name);
	printk("Node right neighbour: %s\n", node_all[next_id-1].name);
}


void print_nodes(int8_t node_num) {
    if (node_num == -1) {
        sys_snode_t* node = sys_slist_peek_head(&node_list);
        while (true) {
            int node_number = *((struct data_node*)node)->data;
            print_node_info(node_number);
            print_node_neighbours(node_number);
            if (node != sys_slist_peek_tail(&node_list)) {
                node = sys_slist_peek_next(node);
            } else {
                break;
            }
        }
    } else {
        uint8_t node_id = node_num + 1;
        if (!find_node(node_id)) {
            printk("No such node was found\n");
        } else {
            print_node_info(node_id);
            print_node_neighbours(node_id);
        }
    }
}



/**
 * @brief print the incoming rssi's from mnode to shell
 * @param pack rssi data packet
*/
void print_rssi(rssiPacket_t pack) {
    // filter out what RSSI are needed and not
    char buf[100];
    

    int64_t msTime = k_uptime_get();
    int64_t sTime = msTime / 1000;

    uint8_t i = 0;

    printk("{\r\n");
    for (uint8_t i = 0; i < 8; i++) {
        memset(buf, 0, sizeof (buf));

        if (find_node(pack.node[i])) {
            sprintf(buf, "  <Time>: %lld seconds\r\n  <RSSI %d>: [%d]\r\n", sTime, i, pack.rssi[i]);
            printk("%s", (char*) buf);
        }
    }
    printk("}\r\n\n");
}

void print_thread(void){
    rssiPacket_t rssiPack;
    rssiPack.rssi[0] = -60;
    rssiPack.rssi[1] = -61;
    rssiPack.rssi[2] = -62;
    rssiPack.rssi[3] = -63;
    rssiPack.rssi[4] = -64;
    rssiPack.rssi[5] = -65;
    rssiPack.rssi[6] = -66;
    rssiPack.rssi[7] = -67;

    while (1) {
        k_msleep(300);
        print_rssi(rssiPack);
    }

}

/**
 * @brief print the incoming address
 * @param pack data packet
*/
void print_addr(bleRssi_t pack) {
    printk("Node %u @ RSSI %d\r\n", pack.node, pack.rssi);
    printk("Addr %s\r\n", (char *)pack.addr);
    for (int i = 0 ; i < BT_ADDR_LE_STR_LEN; i++) {
        printk("%c\r\n", pack.addr[i]);
    }
}

/** 
 * @brief process the incoming struct from the SCU in json format
 * @param rxPacket incoming data packet
 */
void json_process(hciPacket_t rxPacket) {

    char buf[100];
    memset(buf, 0, sizeof (buf));

    int64_t msTime = k_uptime_get();
    int64_t sTime = msTime / 1000;

    if (jsonIndex == 0x00) {
        printk("{\r\n");
    }

    //Check if the incoming data is for orientation
    if (rxPacket.devID == ORIENTATION_ID) {
        if (orientationIndex == 0x00) {
            sprintf(buf, "  <Time>: %lld seconds\r\n  <DID %d>: [%f, ", sTime, rxPacket.devID, rxPacket.msgData);
            orientationIndex++;
        } else if (orientationIndex == 0x01) {
            sprintf(buf, "%f]\r\n", rxPacket.msgData);
            orientationIndex = 0;
        }
    } else {
        sprintf(buf, "  <Time>: %lld seconds\r\n  <DID %d>: [%f]\r\n", sTime, rxPacket.devID, rxPacket.msgData);
    }

    //Print the buffer
    printk("%s", (char*) buf);
    jsonIndex++;

    if (rxPacket.devID == CONT_ID) {
        jsonIndex = 0x00;
        printk("}\r\n\n");
        return;
    }

    //Close the JSON appropriately
    if (jsonIndex == 16 && jsonFlag == 0x01) {
        jsonIndex = 0x00;
        printk("}\r\n\n");
    } else if (jsonFlag == 0x00 && rxPacket.devID != ORIENTATION_ID) {
        jsonIndex = 0x00;
        printk("}\r\n\n");
    } else if (rxPacket.devID == ORIENTATION_ID && jsonFlag != 0x01 && orientationIndex != 0x01) {
        jsonIndex = 0x00;
        printk("}\r\n\n");
    } else if (jsonFlag == 0x02 && orientationIndex != 0x02 && rxPacket.devID != ORIENTATION_ID) {
        jsonIndex = 0x00;
        printk("}\r\n\n"); 
    }
}

/** 
 * @brief request a continuos sampling of altitude and orientation data
 * @param shell pointer to shell struct
 * @param argc number of arguments
 * @param argv argument string array
 */
static void ble_sample(const struct shell *shell, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    hciPacket_t pack;
    pack.preamble = 0xAA;
    pack.msgLen = SIZE_STRUCT;
    pack.msgRw = 0x01;
    pack.newLine = 0x0A;

    //Check for invalid commands
    if ((argc > 3)|| (strlen(argv[0]) > 1) || (strlen(argv[1]) > 1) ||
            (argv[0][0] != 'c')) {
        LOG_ERR("Incorrect command. Command in form 'ble c _'");
        return;
    }

    //Check for invalid commands
    if ((argv[1][0] != 's') && (argv[1][0] != 'p')) {
        LOG_ERR("Incorrect command. Command in form 'ble c _'");
        return;
    }  

    //Set sample rate appropriately
    if (argv[1][0] == 's') {
        pack.devID = CONT_ID;
        pack.msgData = (float) atoi(argv[2]);
        if (pack.msgData < 2 || pack.msgData > 30) {
            LOG_ERR("Error - 2 <= sample rate <= 30");
            return;
        }
        if (pack.msgData == sampleRate) {
            LOG_WRN("Sample already already set to %d", (int) pack.msgData);
        } else {
            sampleRate = pack.msgData;
            LOG_INF("Sample rate set to %d", (int) pack.msgData);
        }
        jsonFlag = 0x02;
        k_msgq_put(&ble_cmd_msgq, &pack, K_NO_WAIT);
    } else if (argv[1][0] == 'p') {
        pack.devID = ZERO_ID;        
        pack.msgData = 0;
        k_msgq_put(&ble_cmd_msgq, &pack, K_NO_WAIT);
    }
}

/** 
 * @brief request a sensor value from the SCU over BLE
 * @param shell pointer to shell struct
 * @param argc number of arguments
 * @param argv argument string array
 */
static void ble_read(const struct shell *shell, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    //Check for invalid commands
    if ((argc != 2 )|| (strlen(argv[0]) > 1) || (strlen(argv[1]) > 2) ||
            (argv[0][0] != 'g')) {
        LOG_ERR("Incorrect command. Command in form 'ble g _'"
                    "where _ is the device ID of the sensor");
        return;
    }

    //Check for incorrect device ID
    int devID = atoi(argv[1]);
    if ((devID <= 0) || (devID > 15)) {
        LOG_ERR("Device ID does not exist");
        return;
    }

    hciPacket_t pack;
    pack.preamble = 0xAA;
    pack.msgLen = SIZE_STRUCT;
    pack.msgRw = 0x01;
    pack.devID = devID;
    pack.msgData = 0x00;
    pack.newLine = 0x0A;

    k_msgq_put(&ble_cmd_msgq, &pack, K_NO_WAIT);    //put into msgq
}

/** 
 * @brief Command to Add a static node for p3b
 * @param shell pointer to shell struct
 * @param argc number of arguments
 * @param argv argument string array
 */
static void cmd_add_static_node(const struct shell *shell, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    //Check for invalid commands
    if ((argc != 4 )|| (strlen(argv[1]) != 6) || (strcmp(argv[0],"add"))) {
        LOG_ERR("Incorrect command. Command in form 'base add 4011-_'"
                    "where _ is the name of the node");
        return;
    }


    char check[6];

    for (int i = 0; i < 5; i++) {
        check[i] = argv[1][i];
    }
    check[5] = '\0';

    if (strcmp(check, "4011-") != 0) {
        LOG_ERR("Incorrect Argument. '-a' or '4011-_'");
        return;
    }
    
    //Check for incorrect node ID
    char ID = argv[1][5];

    if ((ID > 76) || (ID < 65)) {
        LOG_ERR("Incorrect Node - Only Nodes A - L can exist");
        return;
    }

    int node_number = ID - 64;
    int node_x = atoi(argv[2]);
    int node_y = atoi(argv[3]);

    if (find_node(node_number)) {
        printk("Node already exist\n");
        return 0;
	}



    add_node(node_number, node_x, node_y);
	printk("Added the following node\n");
	print_node_info(node_number);
	return 0;

}

/** 
 * @brief Command to remove a static node for p3b
 * @param shell pointer to shell struct
 * @param argc number of arguments
 * @param argv argument string array
 */
static void cmd_remove_static_node(const struct shell *shell, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    //Check for invalid commands
    if ((argc != 2 )|| (strlen(argv[1]) != 6) || (strcmp(argv[0],"rm"))) {
        LOG_ERR("Incorrect command. Command in form 'base rm 4011-_'"
                    "where _ is the name of the node");
        return;
    }


    //Check for incorrect node ID
    char ID = argv[1][5];

    char check[6];

    for (int i = 0; i < 5; i++) {
        check[i] = argv[1][i];
    }
    check[5] = '\0';
    

    if ((ID > 76) || (ID < 65)) {
        LOG_ERR("Incorrect Node - Only Nodes A - L can exist");
        return;
    }
    if (strcmp(check, "4011-") != 0) {
        LOG_ERR("Incorrect Argument. '-a' or '4011-_'");
        return;
    }

    sys_snode_t* node;
    int node_number = ID - 64;

    if ((node = find_node(node_number))) {
        sys_slist_find_and_remove(&node_list, node);
        data_node_free(node);
        printk("Node %d removed\n", node_number);
        return 0;
    } else {
        LOG_ERR("Node doesn't exist");
	}

}




/** 
 * @brief Command to remove a static node for p3b
 * @param shell pointer to shell struct
 * @param argc number of arguments
 * @param argv argument string array
 */
static void cmd_view_static_node(const struct shell *shell, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    //Check for invalid commands
    if ((argc != 2 )|| ((strlen(argv[1]) != 6) && (strlen(argv[1]) != 2)) 
        || (strcmp(argv[0],"view"))) {
        LOG_ERR("Incorrect command. Command in form: 'base view 4011-_'"
                    "where _ is the name of the node; or 'base view -a'");
        return;
    }

    if (sys_slist_is_empty(&node_list)) {
        LOG_ERR("Node list is empty\n");
        return;
    }

    if (strlen(argv[1]) == 2) {
        if (strcmp(argv[1], "-a") == 0) {
        print_nodes(-1);
        } else {
            LOG_ERR("Incorrect Argument. '-a' or '4011-_'");
            return;
        }
    } else {

        //Check for incorrect node ID
        char ID = argv[1][5];

        char check[6];

        for (int i = 0; i < 5; i++) {
            check[i] = argv[1][i];
        }
        check[5] = '\0';
        

        if ((ID > 76) || (ID < 65)) {
            LOG_ERR("Incorrect Node - Only Nodes A - L can exist");
            return;
        }
        if (strcmp(check, "4011-") != 0) {
            LOG_ERR("Incorrect Argument. '-a' or '4011-_'");
            return;
        }

        print_nodes(ID - 65);
    }
}

/**
 * @brief Parse the incoming user data over BLE to create a connection
 * @param data bluetooth struct data
 * @param user_data user data for bluetooth
 * @return bool status of BLE connection
 */
static bool data_parse_cb(struct bt_data *data, void *user_data) {

    bt_addr_le_t *addr = user_data;
    int count = 0;

    if (data->data[0] == 0xFA) {
        hciFlag = 0x01;
    }

    //Check the UUID
    if (data->type == BT_DATA_UUID128_ALL) {
        for (int i = 0; i < data->data_len; i++) {
            if ((data->data[i] == hci_uuid[i]) & !hciFlag) {
                count++;
            } else if ((data->data[i] == mnode_uuid[i]) & hciFlag) {
                count++;
            }
        }
        if (count == UUID_BUFFER_SIZE) {
            int err;
            err = bt_le_scan_stop();    //Stop scanning
            k_msleep(10);
            if (err) {
                LOG_ERR("Scan Stop Failed\r\n");
                return true;
            }

            //Create BLE connection
            struct bt_le_conn_param *connectionParams = BT_LE_CONN_PARAM_DEFAULT;
            err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, connectionParams, &default_conn);
            if (err) {
                LOG_ERR("Connection failed\r\n");
                start_scan();   //Scan again
            }
            return false;
        }
    }
    return true;
}

/**
 * @brief start or stop sending BLE advertising packets
 * @param shell pointer to shell struct
 * @param argc number of arguments
 * @param argv argument string array
 */
static void cmd_adv(const struct shell *shell, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    //Check for invalid commands
    if ((argc > 2) || (strlen(argv[1]) > 1)) {
        LOG_ERR("Incorrect command. Command in form 'adv _'");
        return;
    }
    if ((argv[1][0] != 'o') && (argv[1][0] != 'f')) {
        LOG_ERR("Incorrect command. Command in form 'adv _'");
        return;
    }

    if (argv[1][0] == 'o') {
        //begin sending advertising packets
        ble_start_advertising();
    } else if (argv[1][0] == 'f') {
        //stop sending advertising packets
        if (bt_le_adv_stop()) {
            LOG_ERR("Error when stopping advertising");
        } else {
            LOG_INF("BLE Advertising Stopped");
        }
        
    }
}

/**
 * @brief request the state of the pushbutton on the scu
 * @param shell pointer to shell struct
 * @param argc number of arguments
 * @param argv argument string array
 */
static void cmd_pb(const struct shell *shell, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    //Define the data packet
    hciPacket_t pack;
    pack.preamble = 0xAA;
    pack.msgLen = SIZE_STRUCT;
    pack.msgRw = 0x01;
    pack.msgData = 0x00;
    pack.newLine = 0x0A;

    LOG_INF("Pushbutton data requested");

    //Check for invalid arguments
    if ((argc > 1) || (strlen(argv[0]) > 1)) {
        LOG_ERR("Incorrect number of arguments for 'pb r'\r\n"
                            "Subcommands:\r\n"
                            "  r :Read the state of the pushbutton on the SCU");
        return;
    }

    //Check argument string
    if (argv[0][0] == 'r') {    //read the pushbutton state 
         pack.devID = PB_ID;
         k_msgq_put(&ble_cmd_msgq, &pack, K_NO_WAIT);
    } else {    //invalid argument
        LOG_ERR("Invalid argument'\r\n"
                "Subcommands:\r\n"
                "  r :Read the state of the pushbutton on the SCU"); 
        return;
    }
}

/**
 * @brief write the sample rate for the SCU
 * @param shell pointer to shell struct
 * @param argc number of arguments
 * @param argv argument string array
 */
static void cmd_sample(const struct shell *shell, size_t argc, char** argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    //Define the data packet
    hciPacket_t pack;
    pack.preamble = 0xAA;
    pack.msgLen = SIZE_STRUCT;
    pack.msgRw = 0x01;
    pack.devID = SAMPLE_ID;
    pack.newLine = 0x0A;

    //Check for invalid arguments
    if ((argc > 2) || (strlen(argv[0]) > 1)) {
        LOG_ERR("Incorrect number of arguments for 'sample w'\r\n"
                    "Subcommands:\r\n"
                    "  w :Wrie the sample rate for the SCU");
        return;
    }

    //Check commands
    if (argv[0][0] == 'w') {    //write the sample rate
        pack.msgData = (float) atoi(argv[1]);
        if (pack.msgData == sampleRate) {
            LOG_WRN("Sample already already set to %d", (int) pack.msgData);
        } else {
            sampleRate = pack.msgData;
            LOG_INF("Sample rate set to %d", (int) pack.msgData);
        }
        k_msgq_put(&ble_cmd_msgq, &pack, K_NO_WAIT);
    } else {
        LOG_ERR("Invalid argument'\r\n"
                "Subcommands:\r\n"
                "  w :Write the sample rate for the SCU"); 
        return;
    }
}

/**
 * @brief write the LED values on the AHU
 * @param shell pointer to shell struct
 * @param argc number of arguments
 * @param argv argument string array
 */
static void cmd_ahu_led(const struct shell *shell, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    LOG_ERR("function not available");
}

/**
 * @brief write the LED values on the SCU
 * @param shell pointer to shell struct
 * @param argc number of arguments
 * @param argv argument string array
 */
static void cmd_scu_led(const struct shell *shell, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    //Define the data packet
    hciPacket_t pack;
    pack.preamble = 0xAA;
    pack.msgLen = SIZE_STRUCT;
    pack.msgRw = 0x01;
    pack.newLine = 0x0A;

    //Check arguments for error
    if ((argc != 2) | (strlen(argv[1]) > 1) | (strlen(argv[0]) > 1)) {
        //error
        LOG_ERR("Argument error - use form led s <led_state> <led_num>");
        return;
    }

    //Check second argument for led number
    if (argv[1][0] == '1') {
        pack.devID = LED1_ID;
        LOG_INF("LED1 chosen");
    } else if (argv[1][0] == '0') {
        pack.devID = LED0_ID;
        LOG_INF("LED0 chosen");
    } else {
        //error
        LOG_ERR("LED number invalid - valid numbers are 1 and 0");
        return;
    }

    //Check first argument for on/off/toggle command
    if (argv[0][0] == 'o') {
        pack.msgData = 0x01;
        
        //Logging messages
        if ((pack.devID == LED1_ID) && (ld1State == 0x01)) {
            LOG_WRN("LED1 already on");
        } else if ((pack.devID == LED0_ID) && (ld0State == 0x01)) {
            LOG_WRN("LED0 already on");
        } else {
            if (pack.devID == LED1_ID) {
                ld1State = 0x01;
            } else {
                ld0State = 0x01;
            }
            LOG_INF("Chosen LED turned on");
            k_msgq_put(&ble_cmd_msgq, &pack, K_NO_WAIT);
        }
    } else if (argv[0][0] == 'f') {
        pack.msgData = 0x00;

        //Logging messages
        if ((pack.devID == LED1_ID) && (ld1State == 0x00)) {
            LOG_WRN("LED1 already off");
        } else if ((pack.devID == LED0_ID) && (ld0State == 0x00)) {
            LOG_WRN("LED0 already off");
        } else {
            if (pack.devID == LED1_ID) {
                ld1State = 0x00;
            } else {
                ld0State = 0x00;
            }
            LOG_INF("Chosen LED turned off");
            k_msgq_put(&ble_cmd_msgq, &pack, K_NO_WAIT);
        }
    } else if (argv[0][0] == 't') {
        pack.msgData = 0x02;
        LOG_INF("Chosen LED toggled");
        k_msgq_put(&ble_cmd_msgq, &pack, K_NO_WAIT);
    } else {
        //error
        LOG_ERR("LED state command invalid - valid commands are o, f & t");
        return;
    }
}

/**
 * @brief start or stop scanning for BLE AHU+SCU
 * @param shell pointer to shell struct
 * @param argc number of arguments
 * @param argv argument string array
 */
static void cmd_scan(const struct shell *shell, size_t argc, char **argv) {
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
 * @brief read the all sensor data from the SCU
 * @param shell pointer to shell struct
 * @param argc number of arguments
 * @param argv argument string array
 */
static void cmd_all(const struct shell *shell, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    //Define the data packet
    hciPacket_t pack;
    pack.preamble = 0xAA;
    pack.msgLen = SIZE_STRUCT;
    pack.msgRw = 0x01;
    pack.msgData = 0x00;
    pack.newLine = 0x0A;

    //Check for invalid arguments
    if ((argc != 2) || (strlen(argv[1]) > 1)) {
        LOG_ERR("Incorrect number of commands");
        return;
    }

    if (argv[1][0] == 'o') {
        pack.devID = ALL_ID;
        LOG_INF("All sensor data requested");
        jsonFlag = 0x01;
    } else if (argv[1][0] == 'f') {
        pack.devID = ZERO_ID;
        LOG_INF("Stopping all sensor data");
        jsonFlag = 0x00;
    } else {
        LOG_ERR("Incorrect number of commands");
        return;
    }
    k_msgq_put(&ble_cmd_msgq, &pack, K_NO_WAIT);
}

/**
 * @brief callback function for when BLE has scanned a device for connection
 * @param addr bluetooth address
 * @param rssi rssi of BLE connection
 * @param type type of connection
 * @param ad bluetooth data struct
 */
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad) {

	char addr_str[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

	//Exit if device is not conectable
	if (type != BT_GAP_ADV_TYPE_ADV_IND &&
	    type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
		return;
	}

    bt_data_parse(ad, data_parse_cb, (void*) addr);
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
	if (params->type == BT_GATT_DISCOVER_PRIMARY && bt_uuid_cmp(params->uuid, HCI_AHU_UUID) == 0) {
		LOG_DBG("Found AHU primary service");

		params->uuid = NULL;
		params->start_handle = attr->handle + 1;
		params->type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, params);   //Discover GATT characteristics
		if (err != 0) {
            LOG_ERR("Discovery of GATT Characteristics failed (err %d)", err);
		}

		return BT_GATT_ITER_STOP;
	} else if (params->type == BT_GATT_DISCOVER_PRIMARY && bt_uuid_cmp(params->uuid, HCI_MNODE_UUID) == 0) {
		LOG_DBG("Found MNODE primary service");

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
        
        if (bt_uuid_cmp(chrcGatt->uuid, HCI_CHAR_AHU_UUID) == 0) {
            LOG_DBG("Found AHU Characteristic");

			hci_write_handle = chrcGatt->value_handle;  //Save GATT characteristic handle
            chrcFlag = 0x01;
        } else if (bt_uuid_cmp(chrcGatt->uuid, HCI_CHAR_MNODE_UUID) == 0) {
            LOG_DBG("Found MNODE Characteristic");

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
    if (hciFlag == 0x00) {
        gattDiscoverParams.uuid = hci_uuid_ptr;
    } else if (hciFlag == 0x01) {
        gattDiscoverParams.uuid = mnode_uuid_ptr;
    }	
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

		start_scan(); 
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

	start_scan();   //start scanning again
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
	rssiPacket_t rssiPack;
    bleRssi_t addrPack;

    uint8_t *buffer = buf;
	
	//Parse data for message data. Check preamble
	if (buffer[0] == 0xAA) {
		for (int i = 0; i < len; i++) {
			switch (i) {
				case 0:
					pack.preamble = buffer[i];
				case 1:
					pack.msgLen = buffer[i];
				case 2:
					pack.msgRw = buffer[i];
				case 3:
					pack.devID = buffer[i];
				case 4:
					memcpy(&(pack.msgData), buffer + i, sizeof (uint8_t) * 4);
				case 8:
					pack.newLine = buffer[i];
			}
            json_process(pack);
		}
	} else if (buffer[0] == 0xBB) {
		for (int i = 0; i < len; i++) {
			switch (i) {
				case 0:
					rssiPack.preamble = buffer[i];
				case 1:
					rssiPack.msgLen = buffer[i];
				case 2:
					memcpy(&(rssiPack.rssi), buffer + i, sizeof (rssiPack.rssi));	
				case 10:
                    memcpy(&(rssiPack.node), buffer + i, sizeof (rssiPack.node));
                case 18:
					rssiPack.newLine = buffer[i];
			}
		}
        print_rssi(rssiPack);
	} else if (buffer[0] == 0xCC) {
        for (int i = 0; i < len; i++) {
            switch (i) {
                case 0:
                    addrPack.preamble = buffer[i];
                case 1:
                    memcpy(&(addrPack.addr), buffer + i, sizeof (char) * BT_ADDR_LE_STR_LEN);
                case 2:
                    addrPack.rssi = buffer[i];
                case 3:
                    addrPack.node = buffer[i];
            }
        }
        print_addr(addrPack);
    }
    
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
BT_GATT_SERVICE_DEFINE(bsu_ahu_service, BT_GATT_PRIMARY_SERVICE(HCI_BSU_UUID),
	BT_GATT_CHARACTERISTIC(HCI_CHAR_BSU_UUID,
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
void bsu_shell_init() {

    //Create and initialise the 'ble' commands
    SHELL_STATIC_SUBCMD_SET_CREATE(ble_ctrl, SHELL_CMD(g, NULL, "Request a sensor reading from the SCU", ble_read),
                                                SHELL_CMD(c, NULL, "Request continuous sampling of orientation and altitude data", ble_sample),
                                                SHELL_SUBCMD_SET_END);
    SHELL_CMD_REGISTER(ble, &ble_ctrl, "Request a sensor reading from the SCU over BLE", NULL);

    //Base Handler Commands for the linked list
    SHELL_STATIC_SUBCMD_SET_CREATE(node_add, SHELL_CMD(add, NULL, "Add a node to the linked list", cmd_add_static_node),
                                                SHELL_CMD(rm, NULL, "Remove a node from the linkedlist", cmd_remove_static_node),
                                                SHELL_CMD(view, NULL, "View nodes from the linkedlist", cmd_view_static_node),
                                                SHELL_SUBCMD_SET_END);
    SHELL_CMD_REGISTER(base, &node_add, "Base handler for the linked list", NULL);



    //Create and initialise 'pb' commands
    SHELL_STATIC_SUBCMD_SET_CREATE(pb_ctrl, SHELL_CMD(r, NULL, "Request the  pushbutton on the SCU", cmd_pb),
                                                    SHELL_SUBCMD_SET_END);
    SHELL_CMD_REGISTER(pb, &pb_ctrl, "Read or Write the state of the pushbutton on the SCU", NULL);

    //Create and initialise 'sample' commands
    SHELL_STATIC_SUBCMD_SET_CREATE(sample_ctrl, SHELL_CMD(w, NULL, "Write a sample time for the SCU", cmd_sample),
                                                    SHELL_SUBCMD_SET_END);
    SHELL_CMD_REGISTER(sample, &sample_ctrl, "Read or Write the sample time of the SCU", NULL);

    //Create and initialise the 'led' commands
    SHELL_STATIC_SUBCMD_SET_CREATE(led_rgb_ctrl, SHELL_CMD(r, NULL, "Set the AHUs LED red", NULL), 
                                                SHELL_CMD(g, NULL, "Set the AHUs LED green", NULL),
                                                SHELL_CMD(b, NULL, "Set the AHUs LED blue", NULL),
                                                SHELL_SUBCMD_SET_END);
    SHELL_STATIC_SUBCMD_SET_CREATE(led_a_oft_ctrl, SHELL_CMD(o, &led_rgb_ctrl, "Turn the AHUs LED on", cmd_ahu_led), 
                                                SHELL_CMD(f, &led_rgb_ctrl, "Turn the AHUs LED off", cmd_ahu_led),
                                                SHELL_CMD(t, &led_rgb_ctrl, "Toggle the AHUs LED", cmd_ahu_led),
                                                SHELL_SUBCMD_SET_END);
    SHELL_STATIC_SUBCMD_SET_CREATE(led_s_oft_ctrl, SHELL_CMD(o, NULL, "Turn the chosen SCU LED on", cmd_scu_led), 
                                                SHELL_CMD(f, NULL, "Turn the chosen SCU LED off", cmd_scu_led),
                                                SHELL_CMD(t, NULL, "Toggle the chosen SCH LED", cmd_scu_led),
                                                SHELL_SUBCMD_SET_END);
    SHELL_STATIC_SUBCMD_SET_CREATE(led_ctrl, SHELL_CMD(a, &led_a_oft_ctrl, "Control the AHU LED", NULL),
                                                SHELL_CMD(s, &led_s_oft_ctrl, "Control the SCU LED", NULL),
                                                SHELL_SUBCMD_SET_END);
    SHELL_CMD_REGISTER(led, &led_ctrl, "Control the AHU or SCU led", NULL);

    //Create and initialise the 'all' commands
    SHELL_CMD_REGISTER(all, NULL, "Read all sensor data", cmd_all);

    //Create and initialise the 'scan' commands
    SHELL_CMD_REGISTER(scan, NULL, "Start or stop scanning for the AHU+SCU device", cmd_scan);

    //Create and initialise the 'adv' commands
    SHELL_CMD_REGISTER(adv, NULL, "Start or stop sending BLE advertising packets", cmd_adv);
}




/**
 * @brief Main thread for Base Station Unit's (BSU command) command line interface implementation
 */
void bsu_shell_thread(void) {
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

    bsu_shell_init();   //initialise the shell

    init_nodes();


    hciPacket_t txPacket;

    for (;;) {
        
        //Check if we need to send a message of BLE 
        if (k_msgq_get(&ble_cmd_msgq, &txPacket, K_MSEC(10)) == 0) {
            if (chrcFlag == 0x01) {
                gatt_write(hci_write_handle, &txPacket);
            }
        }

        k_msleep(50);
    }
}

/**
 * @brief Main thread for Base Station Unit's (BSU command) bluetooth interface
 */
void bsu_bt_thread(void) {
    int err;

    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)\r\n", err);
        return;
    }

    LOG_DBG("Bluetooth initialised\r\n");
}

/* Define ahu_shell and ahu_uart threads */
K_THREAD_DEFINE(bsu_shell_thread_tid, BSU_SHELL_THREAD_STACK, bsu_shell_thread, 
        NULL, NULL, NULL, BSU_SHELL_THREAD_PRIORITY, 0, 0);
K_THREAD_DEFINE(bsu_bt_thread_tid, BSU_BT_THREAD_STACK, bsu_bt_thread, 
        NULL, NULL, NULL, BSU_BT_THREAD_PRIORITY, 0, 0);

// K_THREAD_DEFINE(test_print_thread_tid, BSU_BT_THREAD_STACK, print_thread, 
//         NULL, NULL, NULL, BSU_BT_THREAD_PRIORITY, 0, 0);
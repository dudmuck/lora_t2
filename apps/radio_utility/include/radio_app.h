#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define FIRST_CHIP_MENU_ROW        3
#define LAST_CHIP_MENU_ROW          (MAX_MENU_ROWS-5)

#if defined(CONFIG_SEMTECH_SX128X)
    #define MAX_MENU_ROWS             16
    #define MAX_MENU_COLUMNS           6
#elif defined(CONFIG_SEMTECH_SX126X) // or CONFIG_HAS_SEMTECH_SX126X 
	#include <sx126x_driver.h> // app header
	#include <sx126x.h> // lbm header
    //#define MEMSCAN
    #define MAX_MENU_ROWS             17
    #define MAX_MENU_COLUMNS           6
#elif defined(CONFIG_SEMTECH_SX127X)
    #define MAX_MENU_ROWS             16
    #define MAX_MENU_COLUMNS           6
#elif defined(CONFIG_SEMTECH_LR11XX)
    #define MAX_MENU_ROWS             23
    #define MAX_MENU_COLUMNS          14
#else
	#error unknown_radio
#endif

#define PA_OFF_DBM      -127

#include <stdio.h>

#define _ITEM_VALUE      0xbb
#define _ITEM_DROPDOWN   0xaa
#define _ITEM_BUTTON     0xcc
#define _ITEM_TOGGLE     0xdd

#define FLAG_MSGTYPE_ALL            0x07
#define FLAG_MSGTYPE_PKT            0x01
#define FLAG_MSGTYPE_PER            0x02
#define FLAG_MSGTYPE_PING           0x04

typedef struct {
    uint8_t itemType;

    const char * const label0;
    const char * const label1;
    bool (*const read)(void);
    bool (*const push)(void);
} toggle_item_t;

typedef struct {
    uint8_t itemType;

    const char * const label;
    void (*const push)(void);
} button_item_t;

typedef struct {
    uint8_t itemType;

    unsigned width; // num columns printed

    void (*const print)(void);
    bool (*const write)(const char*); // NULL for read-only.  return true: redraw menu
} value_item_t;

typedef struct {
    uint8_t row;
    uint8_t col;
} pos_t;

typedef enum {
    MENUMODE_NONE = 0,
    MENUMODE_ENTRY,
    MENUMODE_DROPDOWN,
    MENUMODE_REDRAW,
    MENUMODE_REINIT_MENU,
} menuMode_e;

typedef struct {
    pos_t pos;  // on screen position, both row & col with 1 starting value
    const char* const label;

    const void* const itemPtr;

    uint8_t flags;

    const void* refreshReadItem;    /* other item to be read after activation */
} menu_t;

typedef struct {
    menuMode_e _mode;
    uint8_t sel_idx;
    const menu_t* sm;
    uint8_t dropdown_col;
} menuState_t;

typedef struct {
    uint8_t itemType;

    const char* const * printed_strs;   // displayed values index from read callback return
    const char* const * selectable_strs;   // choices

    unsigned (*const read)(bool forWriting);
    menuMode_e (*const write)(unsigned);
} dropdown_item_t;

extern const struct device *transceiver;

void log_printf(const char* format, ...);

class Radio {
    public:
		static const char* const chipNum_str;
		static void hw_reset(void);
		static void readChip(void);
		static void clearIrqFlags(void);
		static void test(void);
		static void tx_preamble(void);
		static void tx_carrier(void);
        static void Rx(void);
		static void txPkt(void);
		static float getMHz(void);
		static void setMHz(float);

		static const value_item_t tx_dbm_item;

		static const menu_t* get_modem_menu(void);
		static const menu_t* get_modem_sub_menu(void);
		static const menu_t common_menu[];

		static unsigned pktType_read(bool);
		static menuMode_e pktType_write(unsigned);
		static const char* const pktType_strs[];

		static const char* const opmode_status_strs[];
        static const char* const opmode_select_strs[];
        static unsigned opmode_read(bool);
        static menuMode_e opmode_write(unsigned);

        static uint8_t get_payload_length(void);
        static void set_payload_length(uint8_t);

        static unsigned tx_ramp_read(bool);
        static menuMode_e tx_ramp_write(unsigned);
        static const char* tx_ramp_strs[];

        static void tx_payload_length_print(void);
        static bool tx_payload_length_write(const char*);
#if defined(CONFIG_SEMTECH_SX126X) // or CONFIG_HAS_SEMTECH_SX126X 
		static SX126x radio;
#endif

		static unsigned read_register(unsigned);
        static void write_register(unsigned, unsigned);

        static void tx_dbm_print(void);
        static bool tx_dbm_write(const char*);

		static const menu_t lora_menu[];
		static const menu_t gfsk_menu[];

#if defined(CONFIG_SEMTECH_SX128X)
#elif defined(CONFIG_SEMTECH_SX127X)
#elif defined(CONFIG_SEMTECH_SX126X)
		static sx126x_pa_cfg_params_t pa_config;
		static sx126x_mod_params_gfsk_t mpFSK;
		static sx126x_mod_params_lora_t mpLORA;
		static void write_mpLORA(void);
		static void write_mpFSK(void);
		static sx126x_pkt_params_gfsk_t ppFSK;
		static sx126x_pkt_params_lora_t ppLORA;
		static uint8_t tx_param_buf[];
#elif defined(CONFIG_SEMTECH_LR11XX)
#endif
    private:
#if defined(CONFIG_SEMTECH_SX128X)
        #include "radio_sx128x_private.h"
#elif defined(CONFIG_SEMTECH_SX127X)
        #include "radio_sx127x_private.h"
#elif defined(CONFIG_SEMTECH_SX126X)
        #include "radio_sx126x_private.h"
#elif defined(CONFIG_SEMTECH_LR11XX)
        #include "radio_lr11xx_private.h"
#endif
};

void printf_uart(const char *format, ...);

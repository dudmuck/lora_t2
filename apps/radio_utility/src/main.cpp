#include <main.h>
#include <radio_app.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
//#include <zephyr/drivers/lora.h>
#include <smtc_modem_hal_init.h> // reequired initialization?

/* queue to store up to 10 messages (aligned to 4-byte boundary) */
//K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);

#define UART_DEVICE_NODE			DT_CHOSEN(zephyr_shell_uart)

//#define DEFAULT_RADIO_NODE			DT_ALIAS(lora0)

#define SCROLLING_ROWS		20

#define QUEUE_SIZE 			10

#if defined( SX128X )
#include "ralf_sx128x.h"
#elif defined( SX126X )
#include "ralf_sx126x.h"
#include "sx126x.h"
#elif defined( LR11XX )
#include "ralf_lr11xx.h"
#include "lr11xx_system.h"
// #include "lr11xx_hal_context.h"
#endif

typedef enum {
	EVENT_NONE,
	EVENT_SWEEP_TICK,
	EVENT_DO_NEXT_TX,
	EVENT_MENU_REINIT,
	EVENT_MENU_REDRAW,
	EVENT_MENU_NONE,
	EVENT_START_MENU_ITEM_CHANGE,
	EVENT_NAVIGATE_MENU,
	EVENT_COMMIT_MENU_ITEM_CHANGE,
	EVENT_LAST
} _event_t;

typedef struct {
	_event_t event;
	void *data;
} event_t;

typedef enum {
	MSG_TYPE_PACKET = 0,
	MSG_TYPE_PER,
	MSG_TYPE_PINGPONG
} msgType_e;

typedef struct {
	int row;
	int col;
} tablexy_t;

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

extern ralf_t modem_radio;

/*#if DT_HAS_CHOSEN(lora)
#define LORA_TRANSCEIVER DT_CHOSEN(lora)
#else
#define LORA_TRANSCEIVER DT_NODELABEL(lora0)
#endif
const struct device *context = DEVICE_DT_GET(LORA_TRANSCEIVER);*/
const struct device *transceiver = DEVICE_DT_GET(DT_ALIAS(lora_transceiver));
//static const struct device *const lora_dev = DEVICE_DT_GET(DEFAULT_RADIO_NODE);

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);
struct k_msgq msgq;
static char __aligned(4)
	msgq_buff[QUEUE_SIZE * sizeof(event_t)];

struct {
	float start_MHz;
	float step_MHz;
	float stop_MHz;
	unsigned dwell_ms;
	float MHz;
	k_timer ticker;
	bool ticking;
	//bool tick;
} cf_sweep;

enum _urx_ {
	URX_STATE_NONE = 0,
	URX_STATE_ESCAPE,
	URX_STATE_CSI,
} uart_rx_state;

menuState_t menuState;

// [row][column]
const menu_t* menu_table[MAX_MENU_ROWS][MAX_MENU_COLUMNS];
int8_t StopMenuCols[MAX_MENU_ROWS];

uint8_t entry_buf_idx;
char entry_buf[64];

msgType_e msg_type;

struct _cp_ {
	uint8_t row;
	uint8_t tableCol;
} curpos;

uint8_t botRow;
int regAddr = -1;

volatile struct _flags_ {
	uint8_t ping_master	 : 1; // 0
	uint8_t send_ping		: 1; // 1
	uint8_t send_pong		: 1; // 2
	uint8_t pingpongEnable	: 1; // 3
	//uint8_t do_next_tx		: 1; // 4
} flags;

unsigned tx_ipd_ms;
//Timeout mbedTimeout;
unsigned MaxNumPacket;
unsigned CntPacketTx;
unsigned PacketRxSequencePrev;
unsigned CntPacketRxKO;
unsigned CntPacketRxOK;
unsigned RxTimeOutCount;
unsigned receivedCntPacket;

uint8_t saved_ch;

const uint8_t PingMsg[] = "PING";
const uint8_t PongMsg[] = "PONG";
const uint8_t PerMsg[]	= "PER";

unsigned lfsr;
#define LFSR_INIT		0x1ff

uint8_t get_pn9_byte()
{
	uint8_t ret = 0;
	int xor_out;

	xor_out = ((lfsr >> 5) & 0xf) ^ (lfsr & 0xf);	// four bits at a time
	lfsr = (lfsr >> 4) | (xor_out << 5);	// four bits at a time

	ret |= (lfsr >> 5) & 0x0f;

	xor_out = ((lfsr >> 5) & 0xf) ^ (lfsr & 0xf);	// four bits at a time
	lfsr = (lfsr >> 4) | (xor_out << 5);	// four bits at a time

	ret |= ((lfsr >> 1) & 0xf0);

	return ret;
}

void queue_put(_event_t _ev, void *dp)
{
	int result;
	event_t ev;
	ev.event = _ev;
	ev.data = dp;
	result = k_msgq_put(&msgq, (void *)&ev, K_NO_WAIT);
	if (result != 0)
		LOG_ERR("msgq put %d", result);
}

void uart_rx_timeout(struct k_timer *dummy)
{
	/* escape by itself: abort change on item */
	menuState._mode = MENUMODE_REDRAW;
	queue_put(EVENT_MENU_REDRAW, NULL);

	uart_rx_state = URX_STATE_NONE;
}

K_TIMER_DEFINE(uartRxTimeout, uart_rx_timeout, NULL);

void uart_write_buf(const char *buf, unsigned msg_len)
{
	for (unsigned i = 0; i < msg_len; i++) {
		uart_poll_out(uart_dev, buf[i]);
	}
}

void print_uart(char *buf)
{
	int msg_len = strlen(buf);

	for (int i = 0; i < msg_len; i++) {
		uart_poll_out(uart_dev, buf[i]);
	}
}

void printf_uart(const char *format, ...)
{
	char str[64];
	va_list argList;

	va_start(argList, format);
	vsnprintf(str, sizeof(str), format, argList);
	va_end(argList);
	print_uart(str);
}

char strbuf[255];
void log_printf(const char* format, ...)
{
	va_list arglist;

	// put cursor at last scrolling-area line
	printf_uart("\e[%u;1f", botRow);
	va_start(arglist, format);
	vsprintf(strbuf, format, arglist);
	va_end(arglist);

	print_uart(strbuf);
}

void navigate_dropdown(uint8_t ch)
{
	unsigned n;
	const menu_t* m = menuState.sm;
	const dropdown_item_t* di = (const dropdown_item_t*)m->itemPtr;

	switch (ch) {
		case 'A':	// cursor UP
			if (menuState.sel_idx > 0) {
				menuState.sel_idx--;
			}
			break;
		case 'B':	// cursor DOWN
			if (di->selectable_strs[menuState.sel_idx+1] != NULL)
				menuState.sel_idx++;
			break;
	} // ..switch (ch)

	for (n = 0; di->selectable_strs[n] != NULL; n++) {
		printf_uart("\e[%u;%uf", m->pos.row+n, menuState.dropdown_col);
		if (n == menuState.sel_idx)
			printf_uart("\e[7m");
		printf_uart("%s", di->selectable_strs[n]);
		if (n == menuState.sel_idx)
			printf_uart("\e[0m");
	}
	printf_uart("\e[%u;%uf", m->pos.row + menuState.sel_idx, menuState.dropdown_col + strlen(di->selectable_strs[menuState.sel_idx]));
}

void read_menu_item(const menu_t* m, bool selected)
{
	uint8_t valCol;
	const dropdown_item_t* di = (const dropdown_item_t*)m->itemPtr;

	switch (msg_type) {
		case MSG_TYPE_PACKET:
			if (m->flags & FLAG_MSGTYPE_PKT)
				break;
			else
				return;
		case MSG_TYPE_PER:
			if (m->flags & FLAG_MSGTYPE_PER)
				break;
			else
				return;
		case MSG_TYPE_PINGPONG:
			if (m->flags & FLAG_MSGTYPE_PING)
				break;
			else
				return;
	}

	printf_uart("\e[%u;%uf", m->pos.row, m->pos.col);	// set (force) cursor to row;column
	valCol = m->pos.col;
	if (m->label) {
		printf_uart("%s", m->label);
		valCol += strlen(m->label);
	}
	if (di->itemType == _ITEM_DROPDOWN) {
		if (di->read && di->printed_strs) {
			uint8_t ridx = di->read(false);
			if (selected)
				printf_uart("\e[7m");
			printf_uart("%s", di->printed_strs[ridx]);
			if (selected)
				printf_uart("\e[0m");
		} else if (di->printed_strs) {
			printf_uart("%s", di->printed_strs[0]);
		}
	} else if (di->itemType == _ITEM_VALUE) {
		const value_item_t* vi = (const value_item_t*)m->itemPtr;
		if (vi->print) {
			for (unsigned n = 0; n < vi->width; n++)
				printf_uart(" ");

			printf_uart("\e[%u;%uf", m->pos.row, valCol);	// set (force) cursor to row;column
			if (selected)
				printf_uart("\e[7m");
			vi->print();
			if (selected)
				printf_uart("\e[0m");
		}
	} else if (di->itemType == _ITEM_BUTTON) {
		const button_item_t* bi = (const button_item_t*)m->itemPtr;
		if (bi->label) {
			if (selected)
				printf_uart("\e[7m%s\e[0m", bi->label);
			else
				printf_uart("%s", bi->label);
		}
	} else if (di->itemType == _ITEM_TOGGLE) {
		const toggle_item_t* ti = (const toggle_item_t *)m->itemPtr;
		bool on = ti->read();
		if (ti->label1) {
			const char* const cptr = on ? ti->label1 : ti->label0;
			if (selected)
				printf_uart("\e[7m%s\e[0m", cptr);
			else
				printf_uart("%s", cptr);
		} else {
			if (on)
				printf_uart("\e[1m");
			if (selected)
				printf_uart("\e[7m");

			printf_uart("%s", ti->label0);

			if (selected || on)
				printf_uart("\e[0m");
		}
	}
	
} // ..read_menu_item()

bool
is_menu_item_changable(uint8_t table_row, uint8_t table_col)
{
	const menu_t* m = menu_table[table_row][table_col];
	const dropdown_item_t* di = (const dropdown_item_t*)m->itemPtr;

	switch (msg_type) {
		case MSG_TYPE_PACKET:
			if (m->flags & FLAG_MSGTYPE_PKT)
				break;
			else
				return false;
		case MSG_TYPE_PER:
			if (m->flags & FLAG_MSGTYPE_PER)
				break;
			else
				return false;
		case MSG_TYPE_PINGPONG:
			if (m->flags & FLAG_MSGTYPE_PING)
				break;
			else
				return false;
	}

	if (di->itemType == _ITEM_DROPDOWN) {
		if (di->selectable_strs == NULL || di->selectable_strs[0] == NULL) {
			log_printf("NULLstrs%u,%u\r\n", table_row, table_col);
			return false;
		}
	} else if (di->itemType == _ITEM_VALUE) {
		const value_item_t* vi = (const value_item_t*)m->itemPtr;
		if (vi->write == NULL)
			return false;
	} else if (di->itemType == _ITEM_BUTTON) {
		const button_item_t* bi = (const button_item_t*)m->itemPtr;
		if (bi->push == NULL)
			return false;
	} else if (di->itemType == _ITEM_TOGGLE) {
		const toggle_item_t * ti = (const toggle_item_t *)m->itemPtr;
		if (ti->push == NULL)
			return false;
	}

	return true;
} // ..is_menu_item_changable()

bool is_item_selectable(const menu_t* m)
{
	const dropdown_item_t* di = (const dropdown_item_t*)m->itemPtr;

	if (di->itemType == _ITEM_BUTTON) {
		const button_item_t* bi = (const button_item_t*)m->itemPtr;
		if (bi->push == NULL)
			return false;
	} else if (di->itemType == _ITEM_TOGGLE) {
		const toggle_item_t* ti = (const toggle_item_t*)m->itemPtr;
		if (ti->push == NULL)
			return false;
	}

	return true;
}

void navigate_menu(uint8_t ch)
{
	read_menu_item(menu_table[curpos.row][curpos.tableCol], false);

	switch (ch) {
		case 'A':	// cursor UP
			if (curpos.row == 0)
				break;

			{	// find previous row up with column
				int8_t row;
				for (row = curpos.row - 1; row >= 0; row--) {
					if (StopMenuCols[row] > -1) {
						curpos.row = row;
						break;
					}
				}
				if (row == 0 && StopMenuCols[0] < 0)
					break;	// nothing found
			}

			if (curpos.tableCol >= StopMenuCols[curpos.row]) {
				curpos.tableCol = StopMenuCols[curpos.row]-1;
			}

			break;
		case 'B':	// cursor DOWN
			if (curpos.row >= MAX_MENU_ROWS)
				break;

			{	// find next row down with column
				uint8_t row;
				for (row = curpos.row + 1; row < MAX_MENU_ROWS; row++) {
					if (StopMenuCols[row] != -1) {
						curpos.row = row;
						break;
					}
				}
				if (row == MAX_MENU_ROWS-1 && StopMenuCols[row] == -1)
					break;	// nothing found
			}

			if (curpos.tableCol >= StopMenuCols[curpos.row]) {
				curpos.tableCol = StopMenuCols[curpos.row]-1;
			}


			break;
		case 'C':	// cursor LEFT
			if (curpos.tableCol >= StopMenuCols[curpos.row]-1)
				break;

			{ // find next row left with editable
				uint8_t tcol;
				for (tcol = curpos.tableCol + 1; tcol < StopMenuCols[curpos.row]; tcol++) {
					if (is_menu_item_changable(curpos.row, tcol)) {
						curpos.tableCol = tcol;
						break;
					}
				}
			}

			break;
		case 'D':	// cursor RIGHT
			if (curpos.tableCol == 0)
				break;

			{
				int8_t tcol;
				for (tcol = curpos.tableCol - 1; tcol >= 0; tcol--) {
					if (is_menu_item_changable(curpos.row, tcol)) {
						curpos.tableCol = tcol;
						break;
					}
				}
			}

			break;
		default:
			//printf_uart("unhancled-csi:%02x\eE", ch);
			break;
	} // ..switch (ch)

	if (!is_item_selectable(menu_table[curpos.row][curpos.tableCol])) {
		int c;
		for (c = 0; c < StopMenuCols[curpos.row]; c++) {
			if (is_item_selectable(menu_table[curpos.row][c])) {
				curpos.tableCol = c;
				break;
			}
		}
		if (c == StopMenuCols[curpos.row])
			return;
	}

#ifdef MENU_DEBUG
	log_printf("table:%u,%u screen:%u,%u	\r\n", curpos.row, curpos.tableCol, 
		menu_table[curpos.row][curpos.tableCol]->pos.row,
		menu_table[curpos.row][curpos.tableCol]->pos.col
	);
#endif /* MENU_DEBUG */

	read_menu_item(menu_table[curpos.row][curpos.tableCol], true);
} // ..navigate_menu

void
start_value_entry(const menu_t* m)
{
	const value_item_t* vi = (const value_item_t*)m->itemPtr;
	uint8_t col = m->pos.col;

	if (m->label)
		col += strlen(m->label);

	printf_uart("\e[%u;%uf", m->pos.row, col);
	for (unsigned i = 0; i < vi->width; i++)
		printf_uart(" ");	// clear displayed value for user entry

	printf_uart("\e[%u;%uf", m->pos.row, col);
	menuState._mode = MENUMODE_ENTRY;
	entry_buf_idx = 0;
}


bool ishexchar(char ch)
{
	if (((ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) || (ch >= '0' && ch <= '9'))
		return true;
	else
		return false;
}


void sweeper(k_timer *dummy)
{
	if (cf_sweep.ticking)
		queue_put(EVENT_SWEEP_TICK, NULL);
}

void tx_preamble_push()
{
	Radio::tx_preamble();
}
const button_item_t tx_preamble_item = { _ITEM_BUTTON, "TX_PREAMBLE", tx_preamble_push };

void sweep_start_print()
{
	printf_uart("%.3fMHz", (double)cf_sweep.start_MHz);
}

bool sweep_start_write(const char* valStr)
{
	sscanf(valStr, "%f", &cf_sweep.start_MHz);
	return false;
}
const value_item_t sweep_start_item = { _ITEM_VALUE, 5, sweep_start_print, sweep_start_write};


void sweep_stop_print()
{
	printf_uart("%.3fMHz", (double)cf_sweep.stop_MHz);
}

bool sweep_stop_write(const char* valStr)
{
	sscanf(valStr, "%f", &cf_sweep.stop_MHz);
	return false;
}
const value_item_t sweep_stop_item = { _ITEM_VALUE, 5, sweep_stop_print, sweep_stop_write};

void sweep_dwell_print()
{
	printf_uart("%ums", cf_sweep.dwell_ms);
}

bool sweep_dwell_write(const char* valStr)
{
	sscanf(valStr, "%u", &cf_sweep.dwell_ms);
	return false;
}
const value_item_t sweep_dwell_item = { _ITEM_VALUE, 5, sweep_dwell_print, sweep_dwell_write};

void tx_carrier_push()
{
	Radio::tx_carrier();
}
const button_item_t tx_carrier_item = { _ITEM_BUTTON, "TX_CARRIER", tx_carrier_push };

bool is_sweep_menu_item(const void* const itemPtr)
{
	if (itemPtr == &sweep_start_item ||
		itemPtr == &sweep_stop_item ||
		itemPtr == &sweep_dwell_item ||
		itemPtr == &tx_carrier_item ||
		itemPtr == &tx_preamble_item
	)
		return true;
	else
		return false;
}

void sweep_service()
{
	const menu_t* m = menu_table[curpos.row][curpos.tableCol];

	if (is_sweep_menu_item(m->itemPtr)) {
		if (cf_sweep.start_MHz != 0 && cf_sweep.stop_MHz > cf_sweep.start_MHz && cf_sweep.dwell_ms > 0) {
			//std::chrono::microseconds us = std::chrono::microseconds(cf_sweep.dwell_ms * 1000);
			cf_sweep.MHz = cf_sweep.start_MHz;
			k_timer_start(&cf_sweep.ticker, K_NO_WAIT, K_MSEC(cf_sweep.dwell_ms)); // cf_sweep.ticker.attach(sweeper, us);
			log_printf("### sweep start ###\r\n");
			cf_sweep.ticking = true;
		}
	} else {
		if (cf_sweep.ticking) {
			k_timer_stop(&cf_sweep.ticker); // cf_sweep.ticker.detach();
			cf_sweep.ticking = false;
		}
	}
}

void commit_menu_item_change()
{
	const menu_t* m = menu_table[curpos.row][curpos.tableCol];
	const dropdown_item_t* di = (const dropdown_item_t*)m->itemPtr;

	if (di->itemType == _ITEM_DROPDOWN) {
		menuState._mode = di->write(menuState.sel_idx);

		if (menuState._mode == MENUMODE_NONE)
			queue_put(EVENT_MENU_NONE, NULL);
		else if (menuState._mode == MENUMODE_REDRAW)
			queue_put(EVENT_MENU_REDRAW, NULL);
		else if (menuState._mode == MENUMODE_REINIT_MENU)
			queue_put(EVENT_MENU_REINIT, NULL);

		printf_uart("\e[%u;%uf", m->pos.row, m->pos.col-2);
	} else if (di->itemType == _ITEM_VALUE) {
		const value_item_t* vi = (const value_item_t*)m->itemPtr;
		/* commit value entry */
		if (vi->write) {
			if (vi->write(entry_buf)) {
				menuState._mode = MENUMODE_REDRAW;
				queue_put(EVENT_MENU_REDRAW, NULL);
			} else {
				menuState._mode = MENUMODE_NONE;
				queue_put(EVENT_MENU_NONE, NULL);
			}
		} else {
			menuState._mode = MENUMODE_NONE;
			queue_put(EVENT_MENU_NONE, NULL);
		}

		if (menuState._mode == MENUMODE_NONE) {
			read_menu_item(menu_table[curpos.row][curpos.tableCol], true);
		}
	}

} // ..commit_menu_item_change()

void refresh_item_in_table(const void* item)
{
	unsigned table_row;

	if (item == NULL)
		return;

	for (table_row = 0; table_row < MAX_MENU_ROWS; table_row++) {
		int table_col;
		for (table_col = 0; table_col < StopMenuCols[table_row]; table_col++) {
			//log_printf("%u %u %p\r\n", table_row, table_col, menu_table[table_row][table_col]->itemPtr);
			if (item == menu_table[table_row][table_col]->itemPtr) {
				read_menu_item(menu_table[table_row][table_col], false);
				return;
			}
		}
	}
}

void start_menu_item_change()
{
	const menu_t* m = menu_table[curpos.row][curpos.tableCol];
	const dropdown_item_t* di = (const dropdown_item_t*)m->itemPtr;
	bool checkRefresh = false;

	if (di->itemType == _ITEM_DROPDOWN && di->selectable_strs) {
		menuState.dropdown_col = m->pos.col;
		unsigned n, sidx = 0;
		/* start dropdown */
		if (di->read)
			sidx = di->read(true);

		if (m->label)
			menuState.dropdown_col += strlen(m->label);

		for (n = 0; di->selectable_strs[n] != NULL; n++) {
			uint8_t col = menuState.dropdown_col;
			bool leftPad = false;
			if (col > 3 && n > 0) { // dropdown left side padding
				col -= 2;
				leftPad = true;
			}
			printf_uart("\e[%u;%uf", m->pos.row+n, col);
			if (leftPad ) {
				printf_uart("  ");
			}
			if (n == sidx)
				printf_uart("\e[7m");
			printf_uart("%s", di->selectable_strs[n]);
			if (n == sidx)
				printf_uart("\e[0m");
			printf_uart("  ");	// right side padding
		}
		printf_uart("\e[%u;%uf", m->pos.row, menuState.dropdown_col-2);

		menuState._mode = MENUMODE_DROPDOWN;
		menuState.sel_idx = sidx;
		menuState.sm = m;
	} else if (di->itemType == _ITEM_VALUE) {
		/* start value entry */
		start_value_entry(m);
	} else if (di->itemType == _ITEM_BUTTON) {
		const button_item_t* bi = (const button_item_t*)m->itemPtr;
		if (bi->push) {
			bi->push();
			checkRefresh = true;
		}
	} else if (di->itemType == _ITEM_TOGGLE) {
		const toggle_item_t* ti = (const toggle_item_t*)m->itemPtr;
		if (ti->push) {
			bool on = ti->push();
			uint8_t col = m->pos.col;

			if (m->label)
				col += strlen(m->label);

			printf_uart("\e[%u;%uf", m->pos.row, col);
			if (ti->label1) {
				printf_uart("\e[7m%s\e[0m", on ? ti->label1 : ti->label0);
			} else {
				if (on)
					printf_uart("\e[1;7m%s\e[0m", ti->label0);
				else
					printf_uart("\e[7m%s\e[0m", ti->label0);
			}
			checkRefresh = true;
		}
	}

	if (checkRefresh) {
		if (m->refreshReadItem) {
			refresh_item_in_table(m->refreshReadItem);	// read associated
			read_menu_item(m, true);	// restore cursor
		}
	}

} // ..start_menu_item_change()

void serial_callback(uint8_t ch)
{
//#if 0
	char serial_write_buf[4];
	unsigned char serial_write_buf_idx = 0;
	
	switch (uart_rx_state) {
		case URX_STATE_NONE:
			if (ch == 0x1b) {
				if (menuState._mode == MENUMODE_ENTRY) {
					/* abort entry mode */
					menuState._mode = MENUMODE_NONE;
					queue_put(EVENT_MENU_NONE, NULL);
					read_menu_item(menu_table[curpos.row][curpos.tableCol], true);
				} else {
					uart_rx_state = URX_STATE_ESCAPE;
					if (menuState._mode != MENUMODE_NONE) {
						/* is this escape by itself, user wants to abort? */
						//yyy TODO k_timer_start(&uartRxTimeout, K_MSEC(30), K_NO_WAIT); 
						// was before uartRxTimeout.attach(uart_rx_timeout, 30ms);
					}
				}
			} else if (ch == 2) { // ctrl-B
				log_printf("--------------\r\n");
			} else if (ch == '\r') {
				if (menuState._mode == MENUMODE_NONE) {
					queue_put(EVENT_START_MENU_ITEM_CHANGE, NULL); //yyy start_menu_item_change();
				} else {
					entry_buf[entry_buf_idx] = 0;
					queue_put(EVENT_COMMIT_MENU_ITEM_CHANGE, NULL); // yyy event commit_menu_item_change();
				}
				sweep_service();
			} else if (menuState._mode == MENUMODE_ENTRY) {
				if (ch == 8) {
					if (entry_buf_idx > 0) {
						serial_write_buf[serial_write_buf_idx++] = 8;
						serial_write_buf[serial_write_buf_idx++] = ' ';
						serial_write_buf[serial_write_buf_idx++] = 8;
						entry_buf_idx--;
					}
				} else if (ch == 3) {	// ctrl-C
					menuState._mode = MENUMODE_NONE;
					queue_put(EVENT_MENU_NONE, NULL);
				} else if (entry_buf_idx < sizeof(entry_buf)) {
					entry_buf[entry_buf_idx++] = ch;
					serial_write_buf[serial_write_buf_idx++] = ch;
				}
			} else if (menuState._mode == MENUMODE_NONE) {
				if (ishexchar(ch) || ch == '-') {	// characters which start entry
					const value_item_t* vi = (const value_item_t*)menu_table[curpos.row][curpos.tableCol]->itemPtr;
					if (vi->itemType == _ITEM_VALUE) {
						start_value_entry(menu_table[curpos.row][curpos.tableCol]);
						entry_buf[entry_buf_idx++] = ch;
						serial_write_buf[serial_write_buf_idx++] = ch;
					}
				} else if (ch == 'r') {
					menuState._mode = MENUMODE_REDRAW;
					queue_put(EVENT_MENU_REDRAW, NULL);
				} else if (ch == '.') {
					Radio::test();
				}

			}
			break;
		case URX_STATE_ESCAPE:
			//yyy TODO k_timer_stop(&uartRxTimeout); // uartRxTimeout.detach();
			if (ch == '[')
				uart_rx_state = URX_STATE_CSI;
			else {
#ifdef MENU_DEBUG
				log_printf("unhancled-esc:%02x\r\n", ch);
#endif /* MENU_DEBUG */
				uart_rx_state = URX_STATE_NONE;
			}
			break;
		case URX_STATE_CSI:
			if (menuState._mode == MENUMODE_NONE) {
				saved_ch = ch;
				queue_put(EVENT_NAVIGATE_MENU, &saved_ch);	//navigate_menu(ch);
			} else if (menuState._mode == MENUMODE_DROPDOWN)
				navigate_dropdown(ch);

			uart_rx_state = URX_STATE_NONE;
			//printf_uart("\e[18;1f");	// set (force) cursor to row;column
			break;
	} // ..switch (uart_rx_state)
	
	//yyyfflush(stdout);

	if (serial_write_buf_idx > 0) {
		uart_write_buf(serial_write_buf, serial_write_buf_idx);
	}
	
	//pc.sync();
//#endif /* #if 0 */
}

void hw_reset_push()
{
	Radio::hw_reset();

	Radio::readChip();
	menuState._mode = MENUMODE_REINIT_MENU;
	queue_put(EVENT_MENU_REINIT, NULL);
}
const button_item_t hw_reset_item = { _ITEM_BUTTON, "hw_reset", hw_reset_push };

const dropdown_item_t pktType_item = { _ITEM_DROPDOWN, Radio::pktType_strs, Radio::pktType_strs, Radio::pktType_read, Radio::pktType_write};

const dropdown_item_t opmode_item = { _ITEM_DROPDOWN, Radio::opmode_status_strs, Radio::opmode_select_strs, Radio::opmode_read, Radio::opmode_write };

void clearIrqs_push()
{
	Radio::clearIrqFlags();
}
const button_item_t clearirqs_item = { _ITEM_BUTTON, "clearIrqs", clearIrqs_push };

const char* const msgType_strs[] = {
	"PACKET ",
	"PER	",
	"PINGPONG",
	NULL
};

unsigned msgType_read(bool fw)
{
	return msg_type;
}

menuMode_e msgType_write(unsigned widx)
{
	msg_type = (msgType_e)widx;

	if (msg_type == MSG_TYPE_PER) {
		flags.pingpongEnable = 0;
		if (Radio::get_payload_length() < 7)
			Radio::set_payload_length(7);
	} else if (msg_type == MSG_TYPE_PINGPONG) {
		if (Radio::get_payload_length() != 12)
			Radio::set_payload_length(12);
	} else if (msg_type == MSG_TYPE_PACKET) {
		flags.pingpongEnable = 0;
	}

	return MENUMODE_REINIT_MENU;
}

const dropdown_item_t msgType_item = { _ITEM_DROPDOWN, msgType_strs, msgType_strs, msgType_read, msgType_write};

void frf_print()
{
	float MHz;
/*#ifdef CONFIG_SEMTECH_SX127X
	MHz = Radio::radio.get_frf_MHz();
#else
	MHz = Radio::radio.getMHz();
#endif*/
	MHz = Radio::getMHz();
	printf_uart("%.3fMHz", (double)MHz);
}
bool frf_write(const char* valStr)
{
	float MHz;
	sscanf(valStr, "%f", &MHz);
/*#ifdef CONFIG_SEMTECH_SX127X
	Radio::radio.set_frf_MHz(MHz);
#else
	Radio::radio.setMHz(MHz);
#endif*/
	Radio::setMHz(MHz);
	return false;
}
const value_item_t frf_item = { _ITEM_VALUE, 14, frf_print, frf_write };

const button_item_t chipNum_item = { _ITEM_BUTTON, Radio::chipNum_str, NULL};

const dropdown_item_t tx_ramp_item = { _ITEM_DROPDOWN, Radio::tx_ramp_strs, Radio::tx_ramp_strs, Radio::tx_ramp_read, Radio::tx_ramp_write };

void botrow_print()
{
	printf_uart("%u", botRow);
}
bool botrow_write(const char* str)
{
	unsigned n;
	sscanf(str, "%u", &n);
	botRow = n;

	printf_uart("\e[%u;%ur", MAX_MENU_ROWS, botRow); // set scrolling region

	return false;
}
const value_item_t bottomRow_item = { _ITEM_VALUE, 4, botrow_print, botrow_write };

const value_item_t Radio::tx_dbm_item = { _ITEM_VALUE, 7, Radio::tx_dbm_print, Radio::tx_dbm_write };

const menu_t common_menu[] = {
	{ {1, 1},		NULL,	&hw_reset_item, FLAG_MSGTYPE_ALL },
	{ {1, 13}, "packetType:",	&pktType_item, FLAG_MSGTYPE_ALL },
	{ {1, 33},		NULL,	&msgType_item, FLAG_MSGTYPE_ALL },
	{ {1, 43},		NULL,	&chipNum_item, FLAG_MSGTYPE_ALL },
	{ {1, 61},"bottomRow:",	 &bottomRow_item, FLAG_MSGTYPE_ALL },
	{ {2, 1},		NULL,		&frf_item, FLAG_MSGTYPE_ALL },
	{ {2, 15},	 "TX dBm:", &Radio::tx_dbm_item, FLAG_MSGTYPE_ALL },
	{ {2, 30},	"ramp us:",	&tx_ramp_item, FLAG_MSGTYPE_ALL },
	{ {2, 45},		NULL,	 &clearirqs_item, FLAG_MSGTYPE_ALL },
	{ {2, 55},	 "opmode:",		&opmode_item, FLAG_MSGTYPE_ALL },
	{ {0, 0}, NULL, NULL }
};

void pingpong_stop_push ()
{
	flags.pingpongEnable = 0;
}
const button_item_t pingpong_stop_item = { _ITEM_BUTTON, "STOP", pingpong_stop_push };

void pingpong_start_push()
{
	CntPacketTx = 1;
	CntPacketRxKO = 0;
	CntPacketRxOK = 0;
	RxTimeOutCount = 0;
	receivedCntPacket = 0;

	flags.send_ping = 1;

	flags.ping_master = 1;

	flags.pingpongEnable = 1;
	log_printf("ping start\r\n");
}
const button_item_t pingpong_start_item = { _ITEM_BUTTON, "START", pingpong_start_push };

const menu_t msg_pingpong_menu[] = {
	{ {LAST_CHIP_MENU_ROW+3, 1}, NULL, &pingpong_start_item, FLAG_MSGTYPE_PING },
	{ {LAST_CHIP_MENU_ROW+3, 15}, NULL, &pingpong_stop_item, FLAG_MSGTYPE_PING },
	{ {0, 0}, NULL, NULL }
};

void perrx_push()
{
	PacketRxSequencePrev = 0;
	CntPacketRxKO = 0;
	CntPacketRxOK = 0;
	RxTimeOutCount = 0;

	Radio::Rx();
}
const button_item_t perrx_item = { _ITEM_BUTTON, "PERRX", perrx_push };

void do_next_tx()
{
	Radio::radio.tx_buf[0] = CntPacketTx >> 24;
	Radio::radio.tx_buf[1] = CntPacketTx >> 16;
	Radio::radio.tx_buf[2] = CntPacketTx >> 8;
	Radio::radio.tx_buf[3] = CntPacketTx;

	Radio::radio.tx_buf[4] = PerMsg[0];
	Radio::radio.tx_buf[5] = PerMsg[1];
	Radio::radio.tx_buf[6] = PerMsg[2];

	Radio::txPkt();
}

void next_tx_callback()
{
	queue_put(EVENT_DO_NEXT_TX, NULL);
}

void pertx_push()
{
	CntPacketTx = 1;	// PacketRxSequencePrev initialized to 0 on receiver

	log_printf("do perTx\r\n");

	next_tx_callback();
}
const button_item_t pertx_item = { _ITEM_BUTTON, "PERTX", pertx_push };

void tx_ipd_print()
{
	printf_uart("%u", tx_ipd_ms);
}
bool tx_ipd_write(const char* valStr)
{
	sscanf(valStr, "%u", &tx_ipd_ms);
	return false;
}
value_item_t per_ipd_item = { _ITEM_VALUE, 4, tx_ipd_print, tx_ipd_write };

bool numpkts_write(const char* valStr)
{
	sscanf(valStr, "%u", &MaxNumPacket);
	return false;
}
void numpkts_print()
{
	printf_uart("%u", MaxNumPacket);
}
value_item_t per_numpkts_item = { _ITEM_VALUE, 6, numpkts_print, numpkts_write };

void per_reset_push()
{
	PacketRxSequencePrev = 0;
	CntPacketRxKO = 0;
	CntPacketRxOK = 0;
	RxTimeOutCount = 0;
}
const button_item_t per_clear_item = { _ITEM_BUTTON, "resetCount", per_reset_push };

const menu_t msg_per_menu[] = {
	{ {LAST_CHIP_MENU_ROW+3, 1},		NULL,	&pertx_item, FLAG_MSGTYPE_PER },
	{ {LAST_CHIP_MENU_ROW+3, 10}, "TX IPD ms:",	 &per_ipd_item, FLAG_MSGTYPE_PER },
	{ {LAST_CHIP_MENU_ROW+3, 26},"numPkts:", &per_numpkts_item, FLAG_MSGTYPE_PER },
	{ {LAST_CHIP_MENU_ROW+3, 40},		 NULL,	&perrx_item, FLAG_MSGTYPE_PER, &opmode_item },
	{ {LAST_CHIP_MENU_ROW+3, 50},		 NULL, &per_clear_item, FLAG_MSGTYPE_PER, &opmode_item },
	{ {0, 0}, NULL, NULL }
};

bool sweep_step_write(const char* valStr)
{
	sscanf(valStr, "%f", &cf_sweep.step_MHz);
	return false;
}
void sweep_step_print()
{
	printf_uart("%.3fMHz", (double)cf_sweep.step_MHz);
}
const value_item_t sweep_step_item = { _ITEM_VALUE, 5, sweep_step_print, sweep_step_write};

void rxpkt_push()
{
	Radio::Rx();
	//yyy menuState.mode = MENUMODE_REDRAW;
}
const button_item_t rx_pkt_item = { _ITEM_BUTTON, "RXPKT", rxpkt_push };

void txpkt_push()
{
	Radio::txPkt();
}
const button_item_t tx_pkt_item = { _ITEM_BUTTON, "TXPKT", txpkt_push };

bool tx_payload_write(const char* txt)
{
	unsigned o, i = 0, pktidx = 0;
	while (i < entry_buf_idx) {
		sscanf(entry_buf+i, "%02x", &o);
		printf_uart("%02x ", o);
		i += 2;
		Radio::radio.tx_buf[pktidx++] = o;
		while (entry_buf[i] == ' ' && i < entry_buf_idx)
			i++;
	}
	log_printf("set payload len %u\r\n", pktidx);
	Radio::set_payload_length(pktidx);
	return true;
}
void tx_payload_print()
{
	uint8_t i, len = Radio::get_payload_length();
	for (i = 0; i < len; i++)
		printf_uart("%02x ", Radio::radio.tx_buf[i]);
}
const value_item_t tx_payload_item = { _ITEM_VALUE, 80, tx_payload_print, tx_payload_write};

void regValue_print()
{
	if (regAddr != -1) {
		printf_uart("%x", Radio::read_register(regAddr));
	}
}
bool regValue_write(const char* txt)
{
	unsigned val;
	sscanf(txt, "%x", &val);
	if (regAddr != -1) {
		Radio::write_register(regAddr, val);
	}
	return false;
}
const value_item_t regValue_item = { _ITEM_VALUE, 5, regValue_print, regValue_write};

bool regAddr_write(const char* txt)
{
	sscanf(txt, "%x", &regAddr);
	return false;
}
void regAddr_print()
{
	if (regAddr != -1)
		printf_uart("%x", regAddr);
}
const value_item_t regAddr_item = { _ITEM_VALUE, 5, regAddr_print, regAddr_write};

void pn9_push()
{
	uint8_t i, len = Radio::get_payload_length();
	for (i = 0; i < len; i++)
		Radio::radio.tx_buf[i] = get_pn9_byte();
}
const button_item_t tx_payload_pn9_item = { _ITEM_BUTTON, "PN9", pn9_push };

const value_item_t tx_payload_length_item = { _ITEM_VALUE, 5, Radio::tx_payload_length_print, Radio::tx_payload_length_write};

const menu_t msg_pkt_menu[] = {
	{ {LAST_CHIP_MENU_ROW+1,1}, "tx payload length:", &tx_payload_length_item, FLAG_MSGTYPE_PKT },
	{ {LAST_CHIP_MENU_ROW+1, 25},				 NULL,	&tx_payload_pn9_item, FLAG_MSGTYPE_PKT, &tx_payload_item },
	{ {LAST_CHIP_MENU_ROW+1, 40},		"regAddr:",		&regAddr_item, FLAG_MSGTYPE_PKT, &regValue_item },
	{ {LAST_CHIP_MENU_ROW+1, 53},		"regValue:",		&regValue_item, FLAG_MSGTYPE_PKT, },
	{ {LAST_CHIP_MENU_ROW+2, 1},				 NULL,		&tx_payload_item, FLAG_MSGTYPE_PKT },
	{ {LAST_CHIP_MENU_ROW+3, 1},				 NULL,			&tx_pkt_item, FLAG_MSGTYPE_PKT, &opmode_item },
	{ {LAST_CHIP_MENU_ROW+3, 10},				 NULL,			&rx_pkt_item, FLAG_MSGTYPE_PKT },
	{ {LAST_CHIP_MENU_ROW+3, 20},				 NULL,		&tx_carrier_item, FLAG_MSGTYPE_PKT, &opmode_item },
	{ {LAST_CHIP_MENU_ROW+3, 45},				 NULL,	&tx_preamble_item, FLAG_MSGTYPE_PKT, &opmode_item },
	{ {LAST_CHIP_MENU_ROW+4, 1},	"cf sweep start:",	&sweep_start_item, FLAG_MSGTYPE_PKT },
	{ {LAST_CHIP_MENU_ROW+4, 28},			"step:",		&sweep_step_item, FLAG_MSGTYPE_PKT },
	{ {LAST_CHIP_MENU_ROW+4, 45},			"stop:",		&sweep_stop_item, FLAG_MSGTYPE_PKT },
	{ {LAST_CHIP_MENU_ROW+4, 62},		"dwell_ms:",	&sweep_dwell_item, FLAG_MSGTYPE_PKT },

	{ {0, 0}, NULL, NULL }
};


const menu_t* get_msg_menu()
{
	switch (msg_type) {
		case MSG_TYPE_PACKET:
			return msg_pkt_menu;
		case MSG_TYPE_PER:
			return msg_per_menu;
		case MSG_TYPE_PINGPONG:
			return msg_pingpong_menu;
	}
	return NULL;
}

void
menu_init_(const menu_t* in, tablexy_t* tc)
{
	unsigned n;

	for (n = 0; in[n].pos.row > 0; n++) {
		const menu_t* m = &in[n];
		if (tc->row != m->pos.row - 1) {
			tc->row = m->pos.row - 1;
			tc->col = 0;
		} else
			tc->col++;

		menu_table[tc->row][tc->col] = m;
#ifdef MENU_DEBUG
		printf_uart("table:%u,%u ", tc->row, tc->col);
		printf_uart(" %d<%d? ", StopMenuCols[tc->row], tc->col);
#endif /* MENU_DEBUG */
		if (StopMenuCols[tc->row] < tc->col)
			StopMenuCols[tc->row] = tc->col;
#ifdef MENU_DEBUG
		printf_uart("{%u %u}", tc->row, tc->col);
		printf_uart("in:%p[%u] screen:%u,%u ", in, n, m->pos.row, m->pos.col);
		printf_uart("pos@%p ", &m->pos);
		//printf_uart(" loc:%p ", &in[n].itemPtr);
		if (in[n].itemPtr) {
			const dropdown_item_t* di = (const dropdown_item_t*)m->itemPtr;
			printf_uart(" itemPtr:%p type:%02x ", di, di->itemType);
		}
		printf_uart("stopMenuCols[%u]: %d ", tc->row, StopMenuCols[tc->row]);
		if (m->label)
			printf_uart("label:%s", m->label);
		else
			printf_uart("noLabel");
		printf_uart("\r\n");
#endif /* MENU_DEBUG */
	}
#ifdef MENU_DEBUG
	char ch;
	printf_uart("hit key:");
	pc.read(&ch, 1);
#endif /* MENU_DEBUG */

}

void full_menu_init()
{
	unsigned n;
	const menu_t *m;
	tablexy_t txy;

	txy.row = INT_MAX;
	txy.col = 0;

	for (n = 0; n < MAX_MENU_ROWS; n++) {
		StopMenuCols[n] = -1;
	}

	menu_init_(common_menu, &txy);

	menu_init_(Radio::common_menu, &txy);

	m = Radio::get_modem_menu();
	if (m == NULL) {
		log_printf("NULL-modemMenu\r\n");
		for (;;) asm("nop");
	}
#ifdef MENU_DEBUG
	printf_uart("modemmenuInit\r\n");
#endif
	menu_init_(m, &txy);

	m = Radio::get_modem_sub_menu();
	if (m) {
#ifdef MENU_DEBUG
		printf_uart("modemsubmenuInit\r\n");
#endif
		menu_init_(m, &txy);
	}
#ifdef MENU_DEBUG
	else
		printf_uart("no-modemsubmenu\r\n");
#endif

	m = get_msg_menu();
	if (m == NULL) {
		log_printf("NULL-msgMenu\r\n");
		for (;;) asm("nop");
	}
	menu_init_(m, &txy);

	for (n = 0; n < MAX_MENU_ROWS; n++) {
		if (StopMenuCols[n] != -1)
			StopMenuCols[n]++;
	}
}

void draw_menu()
{
	unsigned table_row;

	for (table_row = 0; table_row < MAX_MENU_ROWS; table_row++) {
		int table_col;
		for (table_col = 0; table_col < StopMenuCols[table_row]; table_col++) {
			read_menu_item(menu_table[table_row][table_col], false);
		} // ..table column iterator
	} // ..table row iterator

	read_menu_item(menu_table[curpos.row][curpos.tableCol], true);
	
	/*yyy fflush(stdout);
	pc.sync();	*/
} // ..draw_menu()

/*
 * Read characters from UART until line end is detected. Afterwards push the
 * data to the message queue.
 */
void serial_cb(const struct device *dev, void *user_data)
{
	uint8_t c;

	if (!uart_irq_update(uart_dev)) {
		return;
	}

	if (!uart_irq_rx_ready(uart_dev)) {
		return;
	}

	/* read until FIFO empty */
	while (uart_fifo_read(uart_dev, &c, 1) == 1) {
		serial_callback(c);
	}
}

/* yyy TODO txDone() callback */

int app_main(void)
{
	//yyy unsigned cnt = 0;
	//struct lora_modem_config config;

	if (!device_is_ready(uart_dev)) {
		printk("UART device not found!");
		return 0;
	}
	k_timer_init(&cf_sweep.ticker, sweeper, NULL);
	k_msgq_init(&msgq, msgq_buff, sizeof(event_t), QUEUE_SIZE);

	lfsr = LFSR_INIT;
	msg_type = MSG_TYPE_PACKET;
	uart_rx_state = URX_STATE_NONE;
	botRow = MAX_MENU_ROWS + SCROLLING_ROWS;

	printf_uart("\e[2J"); // erase entire screen
	// zephyr boot message printing is desired prior to our printing
	k_sleep(K_MSEC(500));

	printf_uart("\e[7h"); // enable line wrapping
	printf_uart("\e[%u;%ur", MAX_MENU_ROWS, botRow); // set scrolling region
	printf_uart("\e[2J"); // erase entire screen

	full_menu_init();

	printf_uart("\e[2J"); // erase entire screen

	menuState._mode = MENUMODE_NONE;

	draw_menu();

	curpos.row = 0;
	curpos.tableCol = 0;

	tx_ipd_ms = 100;


/*	if (!device_is_ready(lora_dev)) {
		printk("%s Device not ready", lora_dev->name);
		return 0;
	}*/

	/* configure interrupt and callback to receive data */
	int ret = uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);

	modem_radio.ral.context = transceiver;

	smtc_modem_hal_init(transceiver);

	if (ret < 0) {
		if (ret == -ENOTSUP) {
			printk("Interrupt-driven UART API support not enabled\n");
		} else if (ret == -ENOSYS) {
			printk("UART device does not support interrupt-driven API\n");
		} else {
			printk("Error setting UART callback: %d\n", ret);
		}
		return 0;
	}

	uart_irq_rx_enable(uart_dev);

	/* testing lora config... */
	/*config.frequency = 915000000;
	config.bandwidth = BW_125_KHZ;
	config.datarate = SF_10;
	config.preamble_len = 8;
	config.coding_rate = CR_4_5;
	config.iq_inverted = false;
	config.public_network = false;
	config.tx_power = 4;
	config.tx = true;

	ret = lora_config(lora_dev, &config);
	if (ret < 0) {
		printk("LoRa config failed");
		return 0;
	}*/
	/* ...testing lora config */

	/*printf_uart("printf-hello world\r\n");
	printk("printtk-hello world\r\n");*/

	while (true) {
		event_t event;
		int err = k_msgq_get(&msgq, &event, K_FOREVER);

		if (err != 0) {
			LOG_ERR("%d = k_msgq_get", err);
			continue;
		}

		/* yyy TODO -->
		if (Radio::service(menuState.mode == MENUMODE_NONE ? LAST_CHIP_MENU_ROW : -1)) {
			read_menu_item(menu_table[curpos.row][curpos.tableCol], true);
		}*/

		switch (event.event) {
			case EVENT_NONE:
				break;
			case EVENT_DO_NEXT_TX:
				do_next_tx();
				break;
			case EVENT_MENU_NONE:
				break;
			case EVENT_MENU_REINIT:
				full_menu_init();
				queue_put(EVENT_MENU_REDRAW, NULL);
				break;
			case EVENT_MENU_REDRAW:
				printf_uart("\e[%u;%ur", MAX_MENU_ROWS, botRow); // set scrolling region, if terminal started after
				printf_uart("\e[2J");
				menuState._mode = MENUMODE_NONE;
				draw_menu();
				break;
			case EVENT_SWEEP_TICK:
				// zephyr Radio.SetChannel()
#ifdef CONFIG_SEMTECH_SX127X
				Radio::radio.set_frf_MHz(cf_sweep.MHz);
#else
				//yyy TODO Radio::radio.setMHz(cf_sweep.MHz);
#endif
				cf_sweep.MHz += cf_sweep.step_MHz;
				if (cf_sweep.MHz >= cf_sweep.stop_MHz)
					cf_sweep.MHz = cf_sweep.start_MHz;
				break;
			case EVENT_START_MENU_ITEM_CHANGE:
				start_menu_item_change();
				break;
			case EVENT_NAVIGATE_MENU:
				{
					uint8_t *ch_ptr = (uint8_t*)event.data;
					navigate_menu(*ch_ptr);
				}
				break;
			case EVENT_COMMIT_MENU_ITEM_CHANGE:
				commit_menu_item_change();
				break;
			case EVENT_LAST:
				break;
		} // ..switch (event)
	} // ..while (true)
}

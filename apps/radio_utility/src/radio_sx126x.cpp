#include <radio_app.h>
#include <sx126x.h>

LOG_MODULE_REGISTER(radio_sx126x, LOG_LEVEL_INF);

SX126x Radio::radio;
unsigned Radio::tcxoDelayTicks;
const char* const Radio::chipNum_str = "SX126x";
uint8_t Radio::pa_config_buf[4];
//ModulationParams_t Radio::mpFSK, Radio::mpLORA;
sx126x_mod_params_lora_t Radio::mpLORA;
sx126x_mod_params_gfsk_t Radio::mpFSK;
uint8_t Radio::cadParams[7];
PacketParams_t Radio::ppFSK, Radio::ppLORA;
uint16_t Radio::ppg;
uint8_t Radio::bw_idx;
uint8_t Radio::tx_param_buf[2];

void Radio::write_mpFSK()
{
	sx126x_status_t ret = sx126x_set_gfsk_mod_params(transceiver, &mpFSK);
	if (ret != SX126X_STATUS_OK) {
		log_printf("%d = sx126x_set_gfsk_mod_params", ret);
	}
}

void Radio::write_mpLORA()
{
	sx126x_status_t ret = sx126x_set_lora_mod_params(transceiver, &mpLORA);
	if (ret != SX126X_STATUS_OK) {
		log_printf("%d = sx126x_set_lora_mod_params", ret);
	}
}

bool Radio::lora_sf_write(const char* str)
{
	unsigned sf;
	if (sscanf(str, "%u", &sf) == 1) {
		mpLORA.sf = (sx126x_lora_sf_t)sf;
		//mpLORA.lora.spreadingFactor = sf;
		//yyy TODO radio.xfer(OPCODE_SET_MODULATION_PARAMS, 4, 0, mpLORA.buf);
		//sx126x_status_t sx126x_set_lora_mod_params( const void* context, const sx126x_mod_params_lora_t* params )
		write_mpLORA();
	}
	return false;
}
void Radio::lora_sf_print()
{
	loraConfig0_t conf0;
	conf0.octet = 0;	//yyy TODO radio.readReg(REG_ADDR_LORA_CONFIG0, 1);
	mpLORA.sf = (sx126x_lora_sf_t)conf0.bits.modem_sf;
	printf_uart("%u", conf0.bits.modem_sf);
}
const value_item_t Radio::lora_sf_item = { _ITEM_VALUE, 3, lora_sf_print, lora_sf_write };

static const char* const lora_crs[] = {
	"4/5   ",
	"4/6   ",
	"4/7   ",
	"4/8   ",
	NULL
};
menuMode_e Radio::lora_cr_write(unsigned sidx)
{
	mpLORA.cr = (sx126x_lora_cr_t)sidx;
	//yyy TODO radio.xfer(OPCODE_SET_MODULATION_PARAMS, 4, 0, mpLORA.buf);
	sx126x_status_t ret = sx126x_set_lora_mod_params(transceiver, &mpLORA);
	if (ret != SX126X_STATUS_OK) {
		log_printf("%d = sx126x_set_lora_mod_params", ret);
	}

	return MENUMODE_REDRAW;
}
const dropdown_item_t Radio::lora_cr_item = { _ITEM_DROPDOWN, lora_crs, lora_crs, lora_cr_read, lora_cr_write};


bool Radio::lora_cadtimeout_write(const char* txt)
{
	unsigned n;
	float ticks;

	sscanf(txt, "%u", &n);
	ticks = n / 15.625;
	n = ticks;

	cadParams[4] = n >> 16;
	cadParams[5] = n >> 8;
	cadParams[6] = n;

	return false;
}
void Radio::lora_cadtimeout_print(void)
{
	unsigned n;

	n = cadParams[4];
	n <<= 8;
	n += cadParams[5];
	n <<= 8;
	n += cadParams[6];
	printf_uart("%u", n);
}
const value_item_t Radio::lora_cadtimeout_item = { _ITEM_VALUE, 4, lora_cadtimeout_print, lora_cadtimeout_write};

bool Radio::lora_cadexit_read()
{
	return cadParams[3];
}
bool Radio::lora_cadexit_push()
{
	if (cadParams[3])
		cadParams[3] = 0;
	else
		cadParams[3] = 1;

	//yyy TODO radio.xfer(OPCODE_SET_CAD_PARAM, 7, 0, cadParams);

	return cadParams[3];
}
const toggle_item_t Radio::lora_cadexit_item = { _ITEM_TOGGLE, "CAD_ONLY", "CAD_RX  ", lora_cadexit_read, lora_cadexit_push};

void Radio::lora_cadpnratio_print()
{
	//yyy TODO cadParams[1] = radio.readReg(REG_ADDR_LORA_CAD_PN_RATIO, 1);
	printf_uart("%u", cadParams[1]);
}

bool Radio::lora_cadpnratio_write(const char* txt)
{
	unsigned n;
	sscanf(txt, "%u", &n);
	cadParams[1] = n;
	//yyy TODO radio.xfer(OPCODE_SET_CAD_PARAM, 7, 0, cadParams);
	return false;
}
const value_item_t Radio::lora_cadpnratio_item = { _ITEM_VALUE, 4, lora_cadpnratio_print, lora_cadpnratio_write};


bool Radio::lora_sdmode_push()
{
	sdCfg0_t sdcfg;
	sdcfg.octet = 0;	/// yyy TODO radio.readReg(REG_ADDR_SDCFG0, 1);
	sdcfg.bits.sd_mode ^= 1;
	//yyy TODO radio.writeReg(REG_ADDR_SDCFG0, sdcfg.octet, 1);
	return sdcfg.bits.sd_mode;
}
bool Radio::lora_sdmode_read()
{
	sdCfg0_t sdcfg;
	sdcfg.octet = 0;//yyy TODO radio.readReg(REG_ADDR_SDCFG0, 1);
	return sdcfg.bits.sd_mode;
}
const toggle_item_t Radio::lora_sdmode_item = { _ITEM_TOGGLE, "sd_mode", NULL, lora_sdmode_read, lora_sdmode_push};

unsigned Radio::lora_cr_read(bool forWriting)
{
	loraConfig1_t conf1;
	sx126x_status_t ret = sx126x_read_register(transceiver, REG_ADDR_LORA_CONFIG1, &conf1.octet, 1);
	if (ret != SX126X_STATUS_OK) {
		log_printf("%d = sx126x_read_register", ret);
		return 0;
	}
 
	mpLORA.cr = (sx126x_lora_cr_t)conf1.bits.tx_coding_rate;
	return conf1.bits.tx_coding_rate;
}

void Radio::cad_push()
{
	{
		uint8_t buf[8];
		IrqFlags_t irqEnable;
		irqEnable.word = 0;
		irqEnable.bits.RxDone = 1;
		irqEnable.bits.Timeout = 1;
		irqEnable.bits.CadDetected = 1;
		irqEnable.bits.CadDone = 1;

		buf[0] = 3;//irqEnable.word >> 8;	// enable bits
		buf[1] = 0xff;//irqEnable.word; // enable bits
		buf[2] = irqEnable.word >> 8;	 // dio1
		buf[3] = irqEnable.word;	// dio1
		buf[4] = 0; // dio2
		buf[5] = 0; // dio2
		buf[6] = 0; // dio3
		buf[7] = 0; // dio3
		//yyy TODO radio.xfer(OPCODE_SET_DIO_IRQ_PARAMS, 8, 0, buf);
	}

	//radio.setCAD();
	//yyy TODO xfer(OPCODE_SET_CAD, 0, 0, NULL);
}
const button_item_t Radio::lora_cad_item = { _ITEM_BUTTON, "CAD", cad_push };


bool Radio::lora_headerType_push()
{
	if (ppLORA.lora.HeaderType)
		ppLORA.lora.HeaderType = 0;
	else
		ppLORA.lora.HeaderType = 1;

	//yyy TODO radio.xfer(OPCODE_SET_PACKET_PARAMS, 6, 0, ppLORA.buf);
	return ppLORA.lora.HeaderType;
}
bool Radio::lora_headerType_read()
{
	loraConfig1_t conf1;
	conf1.octet = 0;//yyy TODO radio.readReg(REG_ADDR_LORA_CONFIG1, 1);
	ppLORA.lora.HeaderType = conf1.bits.implicit_header;
	return conf1.bits.implicit_header;
}
const toggle_item_t Radio::lora_headerType_item = { _ITEM_TOGGLE, "EXPLICIT", "IMPLICIT", lora_headerType_read, lora_headerType_push};

bool Radio::lora_cadmin_write(const char* txt)
{
	unsigned n;
	sscanf(txt, "%u", &n);
	cadParams[2] = n;
	//yyy TODO radio.xfer(OPCODE_SET_CAD_PARAM, 7, 0, cadParams);
	return false;
}
void Radio::lora_cadmin_print()
{
	cadParams[2] = 0;//yyy TODO radio.readReg(REG_ADDR_LORA_CAD_MINPEAK, 1);
	printf_uart("%u", cadParams[2]);
}
const value_item_t Radio::lora_cadmin_item = { _ITEM_VALUE, 4, lora_cadmin_print, lora_cadmin_write};

bool Radio::lora_ppg_write(const char* txt)
{
	unsigned val;
	if (sscanf(txt, "%x", &val) == 1) {
		ppg &= 0x0707;
		ppg |= (val & 0xf0) << 8;
		ppg |= (val & 0x0f) << 4;
		//yyy TODO radio.writeReg(REG_ADDR_LORA_SYNC, ppg, 2);
	}
	return false;
}
void Radio::lora_ppg_print()
{
	uint8_t val;
	ppg = 0;//yyy TODO radio.readReg(REG_ADDR_LORA_SYNC, 2);

	val = (ppg >> 8) & 0xf0;
	val |= (ppg & 0xf0) >> 4;
	printf_uart("%02x", val);
}
const value_item_t Radio::lora_ppg_item = { _ITEM_VALUE, 4, lora_ppg_print, lora_ppg_write};

static const char* const lora_bwstrs[] = {
	" 7.81KHz", "10.42KHz", "15.63KHz",
	"20.83KHz", "31.25KHz", "41.67KHz",
	" 62.5KHz", "  125KHz", "  250KHz",
	"  500KHz",
	NULL
};
/*const uint8_t loraBWs[] = {
	LORA_BW_7, LORA_BW_10, LORA_BW_15,
	LORA_BW_20, LORA_BW_31, LORA_BW_41,
	LORA_BW_62, LORA_BW_125, LORA_BW_250,
	LORA_BW_500
};*/
const sx126x_lora_bw_t loraBWs[] = {
	SX126X_LORA_BW_007,
	SX126X_LORA_BW_010,
	SX126X_LORA_BW_015,
	SX126X_LORA_BW_020,
	SX126X_LORA_BW_031,
	SX126X_LORA_BW_041,
	SX126X_LORA_BW_062,
	SX126X_LORA_BW_125,
	SX126X_LORA_BW_250,
	SX126X_LORA_BW_500,
};
unsigned Radio::lora_bw_read(bool forWriting)
{
	unsigned n;
	loraConfig0_t conf0;
	sx126x_status_t ret = sx126x_read_register(transceiver, REG_ADDR_LORA_CONFIG0, &conf0.octet, 1);
	if (ret != SX126X_STATUS_OK) {
		log_printf("%d = sx126x_read_register", ret);
		return 0;
	}
 
	mpLORA.bw = (sx126x_lora_bw_t)conf0.bits.modem_bw;
	for (n = 0; n < sizeof(loraBWs); n++) {
		if (conf0.bits.modem_bw == loraBWs[n]) {
			bw_idx = n;
			return n;
		}
	}
	return sizeof(loraBWs);
}
menuMode_e Radio::lora_bw_write(unsigned sidx)
{
	mpLORA.bw = loraBWs[sidx];
	write_mpLORA();
	bw_idx = sidx;
	return MENUMODE_REDRAW;
}
const dropdown_item_t Radio::lora_bw_item = { _ITEM_DROPDOWN, lora_bwstrs, lora_bwstrs, lora_bw_read, lora_bw_write};

void Radio::lora_pblLen_print()
{
	unsigned val = 0;//yyy TODO = radio.readReg(REG_ADDR_LORA_PREAMBLE_SYMBNB, 2);
	ppLORA.lora.PreambleLengthHi = val >> 8;
	ppLORA.lora.PreambleLengthLo = val;
	printf_uart("%u", val);
}
bool Radio::lora_pblLen_write(const char* txt)
{
	unsigned n;
	if (sscanf(txt, "%u", &n) == 1) {
		ppLORA.lora.PreambleLengthHi = n >> 8;
		ppLORA.lora.PreambleLengthLo = n;
		//yyy TODO radio.xfer(OPCODE_SET_PACKET_PARAMS, 6, 0, ppLORA.buf);
	}
	return false;
}
const value_item_t Radio::lora_pblLen_item = { _ITEM_VALUE, 5, lora_pblLen_print, lora_pblLen_write};

bool Radio::ppmOffset_read()
{
	loraConfig1_t conf1;
	sx126x_status_t ret = sx126x_read_register(transceiver, REG_ADDR_LORA_CONFIG1, &conf1.octet, 1);
	if (ret != SX126X_STATUS_OK) {
		log_printf("%d = sx126x_read_register", ret);
		return 0;
	}

	mpLORA.ldro = conf1.bits.ppm_offset;
	return conf1.bits.ppm_offset;
}
bool Radio::ppmOffset_push()
{
	if (mpLORA.ldro) {
		mpLORA.ldro = 0;
	} else {
		mpLORA.ldro = 1;
	}

	write_mpLORA();
	return mpLORA.ldro;
}
const toggle_item_t Radio::lora_ppmOffset_item = { _ITEM_TOGGLE, "LowDatarateOptimize", NULL, ppmOffset_read, ppmOffset_push};

bool Radio::lora_inviq_push()
{
	if (ppLORA.lora.InvertIQ)
		ppLORA.lora.InvertIQ = 0;
	else
		ppLORA.lora.InvertIQ = 1;

	//yyy TODO radio.xfer(OPCODE_SET_PACKET_PARAMS, 6, 0, ppLORA.buf);
	return ppLORA.lora.InvertIQ;
}
bool Radio::lora_inviq_read()
{
	loraConfig1_t conf1;
	conf1.octet = 0;//yyy TODO radio.readReg(REG_ADDR_LORA_CONFIG1, 1);
	ppLORA.lora.InvertIQ = conf1.bits.rx_invert_iq;
	return conf1.bits.rx_invert_iq;
}
const toggle_item_t Radio::lora_inviq_item = { _ITEM_TOGGLE, "InvertIQ", NULL, lora_inviq_read, lora_inviq_push};

bool Radio::lora_crcon_push()
{
	if (ppLORA.lora.CRCType)
		ppLORA.lora.CRCType = 0;
	else
		ppLORA.lora.CRCType = 1;

	//yyy TODO radio.xfer(OPCODE_SET_PACKET_PARAMS, 6, 0, ppLORA.buf);
	return ppLORA.lora.CRCType;
}
bool Radio::lora_crcon_read()
{
	loraConfig2_t conf2;
	conf2.octet = 0;//yyy TODO radio.readReg(REG_ADDR_LORA_CONFIG2, 1);
	ppLORA.lora.CRCType = conf2.bits.tx_payload_crc16_en;
	return conf2.bits.tx_payload_crc16_en;
}
const toggle_item_t Radio::lora_crcon_item = { _ITEM_TOGGLE, "CrcOn", NULL, lora_crcon_read, lora_crcon_push};

static const char* const lora_cadsymbs[] = {
	" 1",
	" 2",
	" 4",
	" 8",
	"16",
	NULL
};
unsigned Radio::lora_cadsymbs_read(bool forWriting)
{
	cadParams[0] = 0;//yyy TODO radio.readReg(REG_ADDR_LORA_CONFIG9, 1);
	cadParams[0] >>= 5;
	return cadParams[0];
}
menuMode_e Radio::lora_cadsymbs_write(unsigned sidx)
{
	cadParams[0] = sidx;
	//yyy TODO radio.xfer(OPCODE_SET_CAD_PARAM, 7, 0, cadParams);
	return MENUMODE_REDRAW;
}
const dropdown_item_t Radio::lora_cadsymbs_item = { _ITEM_DROPDOWN, lora_cadsymbs, lora_cadsymbs, lora_cadsymbs_read, lora_cadsymbs_write};

const menu_t Radio::lora_menu[] = {
	{ {FIRST_CHIP_MENU_ROW+2, 1}, NULL,	 &lora_bw_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+2, 12}, "sf:",	 &lora_sf_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+2, 20}, "cr:",	 &lora_cr_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+2, 30}, NULL, &lora_ppmOffset_item, FLAG_MSGTYPE_ALL },

	{ {FIRST_CHIP_MENU_ROW+3, 1}, "PreambleLength:", &lora_pblLen_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+3, 22},			 NULL, &lora_headerType_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+3, 32},			 NULL, &lora_crcon_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+3, 39},			 NULL, &lora_inviq_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+3, 49},			"ppg:", &lora_ppg_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+3, 58},			 NULL, &lora_sdmode_item, FLAG_MSGTYPE_ALL },

	{ {FIRST_CHIP_MENU_ROW+4, 1},		 NULL,		&lora_cad_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+4, 5},	"symbols:", &lora_cadsymbs_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+4, 20}, "peak/noise:", &lora_cadpnratio_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+4, 35},		"min:",	 &lora_cadmin_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+4, 45},	 "exit:",	&lora_cadexit_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+4, 62}, "timeout us:", &lora_cadtimeout_item, FLAG_MSGTYPE_ALL },

	{ {0, 0}, NULL, NULL }
};

bool Radio::gfsk_bitrate_write(const char* txt)
{
	unsigned bps;//, br;

	if (sscanf(txt, "%u", &bps) == 1) {
		/*br = 32 * (XTAL_FREQ_HZ / (float)bps);
		mpFSK.gfsk.bitrateHi = br >> 16;
		mpFSK.gfsk.bitrateMid = br >> 8;
		mpFSK.gfsk.bitrateLo = br;
		radio.xfer(OPCODE_SET_MODULATION_PARAMS, 8, 0, mpFSK.buf);*/
		mpFSK.br_in_bps = bps;
		write_mpFSK();
	}
	return false;
}
void Radio::gfsk_bitrate_print()
{
	unsigned d;
	sx126x_status_t ret = sx126x_read_register(transceiver, REG_ADDR_BITRATE, (uint8_t*)&d, 3);
	if (ret != SX126X_STATUS_OK) {
		log_printf("%d = sx126x_read_register", ret);
		return;
	}

	//unsigned d = 0;//yyy TODO radio.readReg(REG_ADDR_BITRATE, 3);
	float f = d / 32.0;
	mpFSK.br_in_bps = XTAL_FREQ_HZ / f;
	printf_uart("%u", (unsigned)(XTAL_FREQ_HZ / f));

	/*mpFSK.gfsk.bitrateHi = d >> 16;
	mpFSK.gfsk.bitrateMid = d >> 8;
	mpFSK.gfsk.bitrateLo = d;*/
}
const value_item_t Radio::gfsk_bitrate_item = { _ITEM_VALUE, 8, gfsk_bitrate_print, gfsk_bitrate_write};

static const char* const gfsk_bts[] = {
	"off", // 0
	"0.3", // 1
	"0.5", // 2
	"0.7", // 3
	"1.0", // 4
	NULL
};
unsigned Radio::gfsk_bt_read(bool forWriting)
{
	shapeCfg_t shapeCfg;
	sx126x_status_t ret = sx126x_read_register(transceiver, REG_ADDR_SHAPECFG, &shapeCfg.octet, 1);
	if (ret != SX126X_STATUS_OK) {
		log_printf("%d = sx126x_read_register", ret);
		return 0;
	}

	mpFSK.pulse_shape = (sx126x_gfsk_pulse_shape_t)shapeCfg.octet;
	if (shapeCfg.bits.pulse_shape)
		return shapeCfg.bits.bt + 1;
	else
		return 0;
}
menuMode_e Radio::gfsk_bt_write(unsigned sidx)
{
	switch (sidx) {
		case 0: mpFSK.pulse_shape = SX126X_GFSK_PULSE_SHAPE_OFF; break;
		case 1: mpFSK.pulse_shape = SX126X_GFSK_PULSE_SHAPE_BT_03; break;
		case 2: mpFSK.pulse_shape = SX126X_GFSK_PULSE_SHAPE_BT_05; break;
		case 3: mpFSK.pulse_shape = SX126X_GFSK_PULSE_SHAPE_BT_07; break;
		case 4: mpFSK.pulse_shape = SX126X_GFSK_PULSE_SHAPE_BT_1; break;
	}
	write_mpFSK();
	return MENUMODE_REDRAW;
}
const dropdown_item_t Radio::gfsk_bt_item = { _ITEM_DROPDOWN, gfsk_bts, gfsk_bts, gfsk_bt_read, gfsk_bt_write};


void Radio::gfsk_pblLen_print()
{
	unsigned n = 0;// yyy TODO radio.readReg(REG_ADDR_FSK_PREAMBLE_TXLEN , 2);
	ppFSK.gfsk.PreambleLengthHi = n << 8; // param1
	ppFSK.gfsk.PreambleLengthLo = n;// param2
	printf_uart("%u", n);
}
bool Radio::gfsk_pblLen_write(const char* txt)
{
	unsigned n;
	if (sscanf(txt, "%u", &n) == 1) {
		ppFSK.gfsk.PreambleLengthHi = n << 8; // param1
		ppFSK.gfsk.PreambleLengthLo = n;// param2
		//yyy TODO radio.xfer(OPCODE_SET_PACKET_PARAMS, 9, 0, ppFSK.buf);
	}
	return false;
}
const value_item_t Radio::gfsk_pblLen_item = { _ITEM_VALUE, 5, gfsk_pblLen_print, gfsk_pblLen_write};

static const char* const rxbw_str[] = {
	"  4.8KHz", "  5.8KHz", "  7.3KHz", "  9.7KHz",
	" 11.7KHz", " 14.6KHz", " 19.5KHz", " 23.4KHz",
	" 29.3KHz", " 39.0KHz", " 46.9KHz", " 58.6KHz",
	" 78.2KHz", " 93.8KHz", "117.3KHz", "156.2KHz",
	"187.2KHz", "234.3KHz", "312.0KHz", "373.6KHz",
	"467.0KHz",
	NULL
};
/*static const uint8_t rx_bws[] = {
	GFSK_RX_BW_4800, GFSK_RX_BW_5800, GFSK_RX_BW_7300, GFSK_RX_BW_9700,
	 GFSK_RX_BW_11700, GFSK_RX_BW_14600, GFSK_RX_BW_19500, GFSK_RX_BW_23400,
	 GFSK_RX_BW_29300, GFSK_RX_BW_39000, GFSK_RX_BW_46900, GFSK_RX_BW_58600,
	 GFSK_RX_BW_78200, GFSK_RX_BW_93800, GFSK_RX_BW_117300, GFSK_RX_BW_156200,
	GFSK_RX_BW_187200, GFSK_RX_BW_234300, GFSK_RX_BW_312000, GFSK_RX_BW_373600,
	GFSK_RX_BW_467000
};*/
static const sx126x_gfsk_bw_t rx_bws[] = {
	SX126X_GFSK_BW_4800,
	SX126X_GFSK_BW_5800,
	SX126X_GFSK_BW_7300,
	SX126X_GFSK_BW_9700,
	SX126X_GFSK_BW_11700,
	SX126X_GFSK_BW_14600,
	SX126X_GFSK_BW_19500,
	SX126X_GFSK_BW_23400,
	SX126X_GFSK_BW_29300,
	SX126X_GFSK_BW_39000,
	SX126X_GFSK_BW_46900,
	SX126X_GFSK_BW_58600,
	SX126X_GFSK_BW_78200,
	SX126X_GFSK_BW_93800,
	SX126X_GFSK_BW_117300,
	SX126X_GFSK_BW_156200,
	SX126X_GFSK_BW_187200,
	SX126X_GFSK_BW_234300,
	SX126X_GFSK_BW_312000,
	SX126X_GFSK_BW_373600,
	SX126X_GFSK_BW_467000,
};
unsigned Radio::gfsk_rxbw_read(bool forWriting)
{
	unsigned n;
	bwSel_t bwSel;
	sx126x_status_t ret = sx126x_read_register(transceiver, REG_ADDR_BWSEL, &bwSel.octet, 1);
	if (ret != SX126X_STATUS_OK) {
		log_printf("%d = sx126x_read_register", ret);
		return 0;
	}
	mpFSK.bw_dsb_param = (sx126x_gfsk_bw_t)bwSel.octet;

	for (n = 0; n < sizeof(rx_bws); n++) {
		if (bwSel.octet == rx_bws[n])
			return n;
	}
	return sizeof(rx_bws);
}
menuMode_e Radio::gfsk_rxbw_write(unsigned sidx)
{
	mpFSK.bw_dsb_param = rx_bws[sidx];
	write_mpFSK();
	return MENUMODE_REDRAW;
}
const dropdown_item_t Radio::gfsk_rxbw_item = { _ITEM_DROPDOWN, rxbw_str, rxbw_str, gfsk_rxbw_read, gfsk_rxbw_write};

void Radio::gfsk_fdev_print()
{
	unsigned d = 1;	//yyy TODO radio.readReg(REG_ADDR_FREQDEV, 3);
	printf_uart("%u", (unsigned)(d * FREQ_STEP));
}

bool Radio::gfsk_fdev_write(const char* txt)
{
	unsigned hz;
	if (sscanf(txt, "%u", &hz) == 1) {
		/*fdev = hz / FREQ_STEP;
		mpFSK.gfsk.fdevHi = fdev >> 16;
		mpFSK.gfsk.fdevMid = fdev >> 8;
		mpFSK.gfsk.fdevLo = fdev;
		radio.xfer(OPCODE_SET_MODULATION_PARAMS, 8, 0, mpFSK.buf);*/
		mpFSK.fdev_in_hz = hz;
		write_mpFSK();
	}
	return false;
}
const value_item_t Radio::gfsk_fdev_item = { _ITEM_VALUE, 8, gfsk_fdev_print, gfsk_fdev_write};

bool Radio::gfsk_fixLen_read()
{
	pktCtrl0_t pktCtrl0;
	pktCtrl0.octet = 0;//yyy TODO radio.readReg(REG_ADDR_FSK_PKTCTRL0, 1);
	ppFSK.gfsk.PacketType = pktCtrl0.bits.pkt_len_format; // param6
	return pktCtrl0.bits.pkt_len_format;
}
bool Radio::gfsk_fixLen_push()
{
	if (ppFSK.gfsk.PacketType)
		ppFSK.gfsk.PacketType = 0;
	else
		ppFSK.gfsk.PacketType = 1;

	//yyy TODO radio.xfer(OPCODE_SET_PACKET_PARAMS, 9, 0, ppFSK.buf);
	return ppFSK.gfsk.PacketType;
}
const toggle_item_t Radio::gfsk_fixLen_item = { _ITEM_TOGGLE,
	"fixed   ",
	"variable",
	gfsk_fixLen_read, gfsk_fixLen_push
};

void Radio::gfsk_swl_print()
{
	//yyy TODO ppFSK.gfsk.SyncWordLength = radio.readReg(REG_ADDR_FSK_SYNC_LEN, 1);// param4
	printf_uart("%u", ppFSK.gfsk.SyncWordLength);
}
bool Radio::gfsk_swl_write(const char* txt)
{
	unsigned n;
	unsigned r;
	r = sscanf(txt, "%u", &n);
	if (r == 1) {
		ppFSK.gfsk.SyncWordLength = n;
		//yyy TODO radio.xfer(OPCODE_SET_PACKET_PARAMS, 9, 0, ppFSK.buf);
	}
	return false;
}
const value_item_t Radio::gfsk_swl_item = { _ITEM_VALUE, 3, gfsk_swl_print, gfsk_swl_write};

static const char* const fsk_detlens[] = {
	" off  ",
	" 8bits",
	"16bits",
	"24bits",
	"32bits",
	NULL
};
unsigned Radio::gfsk_pblDetLen_read(bool forWriting)
{
	pktCtrl1_t pktCtrl1;
	pktCtrl1.octet = 0;	//yyy TODO radio.readReg(REG_ADDR_FSK_PKTCTRL1, 1);
	ppFSK.gfsk.PreambleDetectorLength = pktCtrl1.octet & 0x07;	// param3
	if (pktCtrl1.bits.preamble_det_on)
		return pktCtrl1.bits.preamble_len_rx + 1;
	else
		return 0;
}
menuMode_e Radio::gfsk_pblDetLen_write(unsigned sidx)
{
	if (sidx == 0)
		ppFSK.gfsk.PreambleDetectorLength = 0;
	else
		ppFSK.gfsk.PreambleDetectorLength = sidx + 3;

	//yyy TODO radio.xfer(OPCODE_SET_PACKET_PARAMS, 9, 0, ppFSK.buf);
	return MENUMODE_REDRAW;
}
const dropdown_item_t Radio::gfsk_pblDetLen_item = { _ITEM_DROPDOWN, fsk_detlens, fsk_detlens, gfsk_pblDetLen_read, gfsk_pblDetLen_write};

void Radio::gfsk_syncword_print()
{
	//yyy TODO unsigned addr = REG_ADDR_SYNCADDR;
	uint8_t swl_bits = 0;//yyy TODO radio.readReg(REG_ADDR_FSK_SYNC_LEN, 1);
	if (swl_bits & 7) {
		swl_bits |= 7;
		swl_bits++;
	}
	while (swl_bits > 0) {
		// yyy TODO printf_uart("%02x", (unsigned)radio.readReg(addr++, 1));
		swl_bits -= 8;
	}
}
bool Radio::gfsk_syncword_write(const char* txt)
{
	const char* ptr = txt;
	//yyy TODO unsigned addr = REG_ADDR_SYNCADDR;
	int8_t swl_bits = 0;//yyy TODO radio.readReg(REG_ADDR_FSK_SYNC_LEN, 1);
	if (swl_bits & 7) {
		swl_bits |= 7;
		swl_bits++;
	}
	while (swl_bits > 0) {
		char buf[3];
		unsigned n;
		buf[0] = ptr[0];
		buf[1] = ptr[1];
		buf[2] = 0;
		sscanf(buf, "%x", &n);
		//yyy TODO radio.writeReg(addr++, n, 1);
		ptr += 2;
		swl_bits -= 8;
	}
	return false;
}
const value_item_t Radio::gfsk_syncword_item = { _ITEM_VALUE, 17, gfsk_syncword_print, gfsk_syncword_write};

static const char* const addrcomps[] = {
	"		  off		",
	"NodeAddress		  ",
	"NodeAddress+broadcast",
	NULL
};
unsigned Radio::gfsk_addrcomp_read(bool forWriting)
{
	ppFSK.gfsk.AddrComp = 0;//yyy TODO radio.readReg(REG_ADDR_NODEADDRCOMP, 1);// param5
	return ppFSK.gfsk.AddrComp;
}
menuMode_e Radio::gfsk_addrcomp_write(unsigned sidx)
{
	ppFSK.gfsk.AddrComp = sidx;
	//yyy TODO radio.xfer(OPCODE_SET_PACKET_PARAMS, 9, 0, ppFSK.buf);
	return MENUMODE_REDRAW;
}
const dropdown_item_t Radio::gfsk_addrcomp_item = { _ITEM_DROPDOWN, addrcomps, addrcomps, gfsk_addrcomp_read, gfsk_addrcomp_write};


bool Radio::gfsk_crcinit_write(const char* txt)
{
	//unsigned v;
	/* yyy TODO if (sscanf(txt, "%x", &v) == 1)
		radio.writeReg(REG_ADDR_FSK_CRCINIT, v, 2);*/

	return false;
}
void Radio::gfsk_crcinit_print()
{
	//yyy TODO printf_uart("%04x", (unsigned)radio.readReg(REG_ADDR_FSK_CRCINIT, 2));
}
const value_item_t Radio::gfsk_crcinit_item = { _ITEM_VALUE, 5, gfsk_crcinit_print, gfsk_crcinit_write};

bool Radio::gfsk_nodeadrs_write(const char* txt)
{
	//unsigned v;
	/*yyy TODO if (sscanf(txt, "%x", &v) == 1)
		radio.writeReg(REG_ADDR_NODEADDR, v, 1);*/

	return false;
}
void Radio::gfsk_nodeadrs_print()
{
	//yyy TODO printf_uart("%02x", (unsigned)radio.readReg(REG_ADDR_NODEADDR, 1));
}
const value_item_t Radio::gfsk_nodeadrs_item = { _ITEM_VALUE, 3, gfsk_nodeadrs_print, gfsk_nodeadrs_write};

bool Radio::gfsk_broadcast_write(const char* txt)
{
	//unsigned v;
	/*yyy TODO if (sscanf(txt, "%x", &v) == 1)
		radio.writeReg(REG_ADDR_BROADCAST, v, 1);*/

	return false;
}
void Radio::gfsk_broadcast_print()
{
	//yyy TODO printf_uart("%02x", (unsigned)radio.readReg(REG_ADDR_BROADCAST, 1));
}
const value_item_t Radio::gfsk_broadcast_item = { _ITEM_VALUE, 3, gfsk_broadcast_print, gfsk_broadcast_write};

void Radio::gfsk_crcpoly_print()
{
	//yyy TODO printf_uart("%04x", (unsigned)radio.readReg(REG_ADDR_FSK_CRCPOLY, 2));
}
bool Radio::gfsk_crcpoly_write(const char* txt)
{
	//unsigned v;
	/*yyy TODO if (sscanf(txt, "%x", &v) == 1)
		radio.writeReg(REG_ADDR_FSK_CRCPOLY, v, 2);*/

	return false;
}
const value_item_t Radio::gfsk_crcpoly_item = { _ITEM_VALUE, 5, gfsk_crcpoly_print, gfsk_crcpoly_write};


void Radio::gfsk_whiteInit_print()
{
	PktCtrl1a_t PktCtrl1a;
	PktCtrl1a.word = 0;//yyy TODO radio.readReg(REG_ADDR_FSK_PKTCTRL1A, 2);
	printf_uart("%x", PktCtrl1a.bits.whit_init_val);
}
bool Radio::gfsk_whiteInit_write(const char* txt)
{
	unsigned n;
	PktCtrl1a_t PktCtrl1a;
	PktCtrl1a.word = 0;// yyy TODO radio.readReg(REG_ADDR_FSK_PKTCTRL1A, 2);
	if (sscanf(txt, "%x", &n) == 1) {
		PktCtrl1a.bits.whit_init_val = n;
		//yyy TODO radio.writeReg(REG_ADDR_FSK_PKTCTRL1A, PktCtrl1a.word, 2);
	}
	return false;
}
const value_item_t Radio::gfsk_whiteInit_item = { _ITEM_VALUE, 5, gfsk_whiteInit_print, gfsk_whiteInit_write};


static const char* crctypes[] = {
	"	off   ", // 0
	"1 Byte	", // 1
	"2 Byte	", // 2
	"1 Byte inv", // 3
	"2 Byte inv", // 4
	NULL
};
unsigned Radio::gfsk_crctype_read(bool forWriting)
{
	pktCtrl2_t pktCtrl2;
	pktCtrl2.octet = 0;	//yyy TODO radio.readReg(REG_ADDR_FSK_PKTCTRL2, 1);
	ppFSK.gfsk.CRCType = pktCtrl2.octet & 0x7; // param8
	switch (ppFSK.gfsk.CRCType) {
		case GFSK_CRC_OFF: return 0;
		case GFSK_CRC_1_BYTE: return 1;
		case GFSK_CRC_2_BYTE: return 2;
		case GFSK_CRC_1_BYTE_INV: return 3;
		case GFSK_CRC_2_BYTE_INV: return 4;
		default: return 5;
	}
}
menuMode_e Radio::gfsk_crctype_write(unsigned sidx)
{
	switch (sidx) {
		case 0: ppFSK.gfsk.CRCType = GFSK_CRC_OFF; break;
		case 1: ppFSK.gfsk.CRCType = GFSK_CRC_1_BYTE; break;
		case 2: ppFSK.gfsk.CRCType = GFSK_CRC_2_BYTE; break;
		case 3: ppFSK.gfsk.CRCType = GFSK_CRC_1_BYTE_INV; break;
		case 4: ppFSK.gfsk.CRCType = GFSK_CRC_2_BYTE_INV; break;
	}

	//yyy TODO radio.xfer(OPCODE_SET_PACKET_PARAMS, 9, 0, ppFSK.buf);
	return MENUMODE_REDRAW;
}
const dropdown_item_t Radio::gfsk_crctype_item = { _ITEM_DROPDOWN, crctypes, crctypes, gfsk_crctype_read, gfsk_crctype_write};

bool Radio::gfsk_white_read()
{
	pktCtrl2_t pktCtrl2;
	pktCtrl2.octet = 0;//yyy TODO radio.readReg(REG_ADDR_FSK_PKTCTRL2, 1);
	ppFSK.gfsk.Whitening = pktCtrl2.bits.whit_enable; // param9
	return pktCtrl2.bits.whit_enable;
}
bool Radio::gfsk_white_push()
{
	if (ppFSK.gfsk.Whitening)
		ppFSK.gfsk.Whitening = 0;
	else
		ppFSK.gfsk.Whitening = 1;

	//yyy TODO radio.xfer(OPCODE_SET_PACKET_PARAMS, 9, 0, ppFSK.buf);
	return ppFSK.gfsk.Whitening;
}
const toggle_item_t Radio::gfsk_white_item = { _ITEM_TOGGLE, "Whitening", NULL, gfsk_white_read, gfsk_white_push};


const menu_t Radio::gfsk_menu[] = {
	{ {FIRST_CHIP_MENU_ROW+2, 1},	"bps:", &gfsk_bitrate_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+2, 15},	"bt:",	 &gfsk_bt_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+2, 23}, "rxbw:",	&gfsk_rxbw_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+2, 39}, "fdev:",	&gfsk_fdev_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+2, 53},	 NULL, &gfsk_fixLen_item, FLAG_MSGTYPE_ALL },

	{ {FIRST_CHIP_MENU_ROW+3, 1}, "PreambleLength:", &gfsk_pblLen_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+3, 21}, "PreambleDetectorLength:", &gfsk_pblDetLen_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+3, 51}, "SyncWordLength bits:", &gfsk_swl_item, FLAG_MSGTYPE_ALL },

	{ {FIRST_CHIP_MENU_ROW+4, 1}, "SyncWord:", &gfsk_syncword_item, FLAG_MSGTYPE_ALL },

	{ {FIRST_CHIP_MENU_ROW+5, 1}, "AddrComp:", &gfsk_addrcomp_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+5, 33}, "NodeAdrs:", &gfsk_nodeadrs_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+5, 47}, "broadcast:", &gfsk_broadcast_item, FLAG_MSGTYPE_ALL },

	{ {FIRST_CHIP_MENU_ROW+6, 1}, "crcType:", &gfsk_crctype_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+6, 21}, "crcInit:", &gfsk_crcinit_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+6, 34}, "crcPoly:", &gfsk_crcpoly_item, FLAG_MSGTYPE_ALL },

	{ {FIRST_CHIP_MENU_ROW+7, 1},		 NULL, &gfsk_white_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+7, 12}, "lfsr init:", &gfsk_whiteInit_item, FLAG_MSGTYPE_ALL },
//12345678901234567890123456789012

	{ {0, 0}, NULL, NULL }
};

bool Radio::deviceSel_push()
{
/*	if (pa_config_buf[2])
		pa_config_buf[2] = 0;
	else
		pa_config_buf[2] = 1;*/
	pa_config_buf[2] ^= 1;

	//yyy TODO radio.xfer(OPCODE_SET_PA_CONFIG, 4, 0, pa_config_buf);

	return pa_config_buf[2];
}
bool Radio::deviceSel_read()
{
	/*PaCtrl1b_t PaCtrl1b;
	PaCtrl1b.octet = radio.readReg(REG_ADDR_PA_CTRL1B, 1);
	pa_config_buf[2] = PaCtrl1b.bits.tx_mode_bat; // deviceSel
	return PaCtrl1b.bits.tx_mode_bat;*/
	//yyy TODO
	return 0;
}
const toggle_item_t Radio::deviceSel_item = { _ITEM_TOGGLE, "SX1262", "SX1261", deviceSel_read, deviceSel_push};

bool Radio::tcxo_delay_write(const char *txt)
{
	unsigned ms;
	sscanf(txt, "%u", &ms);
	tcxoDelayTicks = ms * 64;
	return false;
}

void Radio::tcxo_delay_print(void)
{
	printf_uart("%u", tcxoDelayTicks / 64);
}
const value_item_t Radio::tcxo_delay_item = { _ITEM_VALUE, 5, tcxo_delay_print, tcxo_delay_write};

static const char* const paDutyCycles[] = {
	"0",
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	NULL
};
unsigned Radio::paDutyCycle_read(bool forWriting)
{
	/*AnaCtrl6_t AnaCtrl6;
	AnaCtrl6.octet = radio.readReg(REG_ADDR_ANACTRL6, 1);
	pa_config_buf[0] = AnaCtrl6.bits.pa_dctrim_select_ana;
	return AnaCtrl6.bits.pa_dctrim_select_ana;*/
	// yyy TODO
	return 0;
}
menuMode_e Radio::paDutyCycle_write(unsigned sidx)
{
	pa_config_buf[0] = sidx;
	/// yyy TODO radio.xfer(OPCODE_SET_PA_CONFIG, 4, 0, pa_config_buf);
	return MENUMODE_REDRAW;
}
const dropdown_item_t Radio::paDutyCycle_item = { _ITEM_DROPDOWN, paDutyCycles, paDutyCycles, paDutyCycle_read, paDutyCycle_write};

bool Radio::xta_write(const char* txt)
{
	/*unsigned trim;
	if (sscanf(txt, "%x", &trim) == 1)
		radio.writeReg(REG_ADDR_XTA_TRIM, trim, 1);*/

	//yyy TODO
	return false;
}
void Radio::xta_print()
{
	/*uint8_t trim = radio.readReg(REG_ADDR_XTA_TRIM, 1);
	printf_uart("%02x", trim);*/
	//yyy TODO
}
const value_item_t Radio::xta_item = { _ITEM_VALUE, 3, xta_print, xta_write};

void Radio::xtb_print()
{
	/*uint8_t trim = radio.readReg(REG_ADDR_XTB_TRIM, 1);
	printf_uart("%02x", trim);*/
	// yyy TODO
}
bool Radio::xtb_write(const char* txt)
{
	/*unsigned trim;
	if (sscanf(txt, "%x", &trim) == 1)
		radio.writeReg(REG_ADDR_XTB_TRIM, trim, 1);*/

	//yyy TODO
	return false;
}
const value_item_t Radio::xtb_item = { _ITEM_VALUE, 3, xtb_print, xtb_write};

void Radio::tcxo_volts_print(void)
{
	// yyy;
}
bool Radio::tcxo_volts_write(const char *txt)
{
#if 0
	uint8_t buf[4];
	float volts;
	sscanf(txt, "%f", &volts);
	if (volts > 3.15)
		buf[0] = 7;// 3.3v
	else if (volts > 2.85)
		buf[0] = 6; // 3.0v
	else if (volts > 2.55)
		buf[0] = 5; // 2.7v
	else if (volts > 2.3)
		buf[0] = 4; // 2.4v
	else if (volts > 2.3)
		buf[0] = 3; // 2.2v
	else if (volts > 2.0)
		buf[0] = 2; // 1.8v
	else if (volts > 1.65)
		buf[0] = 1; // 1.7v
	else
		buf[0] = 0; // 1.6v

	to_big_endian24(tcxoDelayTicks, buf+1);

	radio.xfer(OPCODE_SET_DIO3_AS_TCXO_CTRL, 4, 0, buf);
	log_printf("set txco %u, %u\r\n", buf[0], tcxoDelayTicks);
#endif /* #if 0 */
	//yyy TODO
	return false;
}
const value_item_t Radio::tcxo_volts_item = { _ITEM_VALUE, 3, tcxo_volts_print, tcxo_volts_write};

static const char* const rxfe_pms[] = {
	"LP  0dB",
	"HP1 2dB",
	"HP2 4dB",
	"HP3 6dB",
	NULL
};
unsigned Radio::rxfe_pm_read(bool)
{
	/*AgcSensiAdj_t agcs;
	agcs.octet = radio.readReg(REG_ADDR_AGC_SENSI_ADJ, 1);
	return agcs.bits.power_mode;*/
	//yyy TODO
	return 0;
}
menuMode_e Radio::rxfe_pm_write(unsigned sidx)
{
	/*AgcSensiAdj_t agcs;
	agcs.octet = radio.readReg(REG_ADDR_AGC_SENSI_ADJ, 1);
	agcs.bits.power_mode = sidx;
	radio.writeReg(REG_ADDR_AGC_SENSI_ADJ, agcs.octet, 1);*/
	//yyy TODO
	return MENUMODE_REDRAW;
}
const dropdown_item_t Radio::rxfe_pm_item = { _ITEM_DROPDOWN, rxfe_pms, rxfe_pms, rxfe_pm_read, rxfe_pm_write};

static const char* const hpMaxs[] = {
	"0",
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	NULL
};
menuMode_e Radio::hpMax_write(unsigned sidx)
{
	pa_config_buf[1] = sidx;
	//yyy TODO radio.xfer(OPCODE_SET_PA_CONFIG, 4, 0, pa_config_buf);
	return MENUMODE_REDRAW;
}
unsigned Radio::hpMax_read(bool forWriting)
{
	/*AnaCtrl7_t AnaCtrl7;
	AnaCtrl7.octet = radio.readReg(REG_ADDR_ANACTRL7, 1);
	pa_config_buf[1] = AnaCtrl7.bits.pa_hp_sel_ana;
	return AnaCtrl7.bits.pa_hp_sel_ana;*/
	//yyy TODO
	return 0;
}
const dropdown_item_t Radio::hpMax_item = { _ITEM_DROPDOWN, hpMaxs, hpMaxs, hpMax_read, hpMax_write};

void Radio::ldo_push()
{
	/*uint8_t buf = 0;
	radio.xfer(OPCODE_SET_REGULATOR_MODE, 1, 0, &buf);*/
	//yyy TODO
	log_printf("-> LDO\r\n");
}
const button_item_t Radio::ldo_item = { _ITEM_BUTTON, "LDO", ldo_push };

void Radio::dcdc_push()
{
	/*uint8_t buf = 1;
	radio.xfer(OPCODE_SET_REGULATOR_MODE, 1, 0, &buf);
	log_printf("-> DC-DC\r\n");*/
	//yyy TODO
}
const button_item_t Radio::dcdc_item = { _ITEM_BUTTON, "DCDC", dcdc_push };

bool Radio::ocp_write(const char* txt)
{
	//float mA;
	/*if (sscanf(txt, "%f", &mA) == 1)
		radio.writeReg(REG_ADDR_OCP, mA / 2.5, 1);*/
	// yyy TODO

	return false;
}

void Radio::ocp_print()
{
	/*uint8_t ocp = radio.readReg(REG_ADDR_OCP, 1);
	printf_uart("%.1f", ocp * 2.5);*/
	// yyy TODO
}
const value_item_t Radio::ocp_item = { _ITEM_VALUE, 5, ocp_print, ocp_write};

const menu_t Radio::common_menu[] = {
	{ {FIRST_CHIP_MENU_ROW, 1}, "deviceSel:", &deviceSel_item, FLAG_MSGTYPE_ALL, &tx_dbm_item },
	{ {FIRST_CHIP_MENU_ROW, 18}, "paDutyCycle:", &paDutyCycle_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW, 33},	 "hpMax:",	 &hpMax_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW, 42},	 "ocp mA:",		 &ocp_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW, 55},		 "XTA:",		 &xta_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW, 62},		 "XTB:",		 &xtb_item, FLAG_MSGTYPE_ALL },

	{ {FIRST_CHIP_MENU_ROW+1, 1}, NULL, &ldo_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+1, 5}, NULL, &dcdc_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+1, 12}, "rxfe power:", &rxfe_pm_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+1, 35}, "tcxoVolts:", &tcxo_volts_item, FLAG_MSGTYPE_ALL },
	{ {FIRST_CHIP_MENU_ROW+1, 50}, "tcxoDelay(ms):", &tcxo_delay_item, FLAG_MSGTYPE_ALL },
#ifdef MEMSCAN
	{ {LAST_CHIP_MENU_ROW, 1}, NULL, &memread_item, FLAG_MSGTYPE_ALL },
	{ {LAST_CHIP_MENU_ROW, 10}, NULL, &memcmp_item, FLAG_MSGTYPE_ALL },
#endif /* MEMSCAN */

	{ {0, 0}, NULL, NULL }
};

// zephyr driver at modules/lib/loramac-node/src/radio/sx126x/sx126x.c
// opcodes at modules/lib/loramac-node/src/radio/sx126x/sx126x.h
// driver header at modules/lib/loramac-node/src/radio/sx126x/sx126x.h
void Radio::tx_carrier()
{
	/*radio.SetDIO2AsRfSwitchCtrl(1);
	antswPower = 1;
	radio.xfer(OPCODE_SET_TX_CARRIER, 0, 0, NULL);*/
	//SX126xSetTxContinuousWave();
	//LBM:
	sx126x_status_t ret = sx126x_set_tx_cw(transceiver);
	if (ret != SX126X_STATUS_OK)
		LOG_ERR("%d = sx126x_set_tx_cw", ret);
}

void Radio::test()
{
}

void Radio::tx_preamble()
{
	/*radio.SetDIO2AsRfSwitchCtrl(1);
	antswPower = 1;
	radio.xfer(OPCODE_SET_TX_PREAMBLE, 0, 0, NULL);*/
	//SX126xSetTxInfinitePreamble();
	//LBM:
	//sx126x_status_t sx126x_set_tx_infinite_preamble( const void* context )
	sx126x_status_t ret = sx126x_set_tx_infinite_preamble(transceiver);
	if (ret != SX126X_STATUS_OK)
		LOG_ERR("%d = sx126x_set_tx_infinite_preamble", ret);
}

const char* const Radio::pktType_strs[] = {
	"GFSK   ",
	"LORA   ",
	"BPSK   ",
	"LRFHSS ",
	NULL
};

menuMode_e Radio::pktType_write(unsigned idx)
{
	/*
	SX126X_PKT_TYPE_GFSK	= 0x00,
	SX126X_PKT_TYPE_LORA	= 0x01,
	SX126X_PKT_TYPE_BPSK	= 0x02,
	SX126X_PKT_TYPE_LR_FHSS = 0x03,
	 */
	//was radio.setPacketType(idx);
	sx126x_status_t ret = sx126x_set_pkt_type(transceiver, /*const sx126x_pkt_type_t pkt_type*/(sx126x_pkt_type_t)idx );
	if (ret != SX126X_STATUS_OK) {
		LOG_ERR("%d = sx126x_set_pkt_type", ret);
		return MENUMODE_NONE;
	} else
		return MENUMODE_REINIT_MENU;
}

unsigned Radio::pktType_read(bool fw)
{
	sx126x_pkt_type_t pkt_type;
	sx126x_status_t ret = sx126x_get_pkt_type(transceiver, &pkt_type);
	if (ret != SX126X_STATUS_OK) {
		log_printf("%d = sx126x_get_pkt_type", ret);
		return 0;
	} else {
		return pkt_type;
	}
}

void Radio::hw_reset()
{
	/* sx126x_reset(struct sx126x_data *dev_data) */
	sx126x_status_t ret = sx126x_reset(transceiver);
	if (ret != SX126X_STATUS_OK)
		LOG_ERR("%d = sx126x_set_pkt_type", ret);
	//log_printf("todo hw_reset\r\n");
}


menuMode_e Radio::opmode_write(unsigned sel)
{
	sx126x_status_t sx_ret = SX126X_STATUS_UNSUPPORTED_FEATURE;
    switch (sel) {
        case 0:
			{
            	//yyy TODO antswPower = 0;
				sx_ret = sx126x_set_sleep(transceiver, SX126X_SLEEP_CFG_WARM_START);
				//sx_ret = sx126x_set_sleep(transceiver, SX126X_SLEEP_CFG_COLD_START);
			}
            break;
        case 1:
            //yyy TODO antswPower = 0;
			sx_ret = sx126x_set_standby(transceiver, SX126X_STANDBY_CFG_RC);
            break;
        case 2:
            //yyy TODO antswPower = 0;
			sx_ret = sx126x_set_standby(transceiver, SX126X_STANDBY_CFG_XOSC);
            break;
        case 3:
            //yyy TODO antswPower = 0;
			sx_ret = sx126x_set_fs(transceiver);
            break;
        case 4:
            //yyy TODO antswPower = 1;
			sx_ret = sx126x_set_rx(transceiver, 0);
            break;
        case 5:
            //yyy TODO antswPower = 1;
            {
				sx_ret = sx126x_set_tx(transceiver, 0);
            }
            break;
    }
	//yyy TODO
	if (sx_ret != SX126X_STATUS_OK) {
		log_printf("%d = sx126x_set_sleep", sx_ret);
	}
	return MENUMODE_REDRAW;
}

void Radio::readChip()
{
	//yyy TODO
}

void Radio::clearIrqFlags()
{
	uint16_t irq_mask = 0x03ff;
	sx126x_status_t ret = sx126x_clear_irq_status(transceiver, irq_mask);
	if (ret != SX126X_STATUS_OK) {
		log_printf("%d = sx126x_clear_irq_status", ret);
	}
}

void Radio::Rx()
{
	//yyy TODO
}

const char* const Radio::opmode_status_strs[] = {
	"<0>	  ", // 0
	"RFU	  ", // 1
	"STBY_RC  ", // 2
	"STBY_XOSC", // 3
	"FS	   ", // 4
	"RX	   ", // 5
	"TX	   ", // 6
	"<7>	  ", // 7
	NULL
};

const char* const Radio::opmode_select_strs[] = {
	"SLEEP	 ", // 0
	"STDBY_RC  ", // 1
	"STDBY_XOSC", // 2
	"FS		", // 3
	"RX		", // 4
	"TX		", // 5
	NULL
};

unsigned Radio::opmode_read(bool forWriting)
{
	sx126x_chip_status_t radio_status;
	sx126x_status_t ret = sx126x_get_status(transceiver, &radio_status);
	if (ret != SX126X_STATUS_OK) {
		log_printf("%d = sx126x_get_status", ret);
		return 0;
	}
	
    if (forWriting) {
        /* translate opmode_status_strs to opmode_select_strs */
        switch (radio_status.chip_mode) {
            case 2: return 1; // STBY_RC
            case 3: return 2; // STBY_XOSC
            case 4: return 3; // FS
            case 5: return 4; // RX
            case 6: return 5; // TX
            default: return 0;
        }
    } else
        return radio_status.chip_mode;
}

void Radio::set_payload_length(uint8_t len)
{
	//yyy TODO
}

uint8_t Radio::get_payload_length()
{
	//yyy TODO
	return 0;
}

void Radio::tx_payload_length_print()
{
	printf_uart("%u", get_payload_length());
}

bool Radio::tx_payload_length_write(const char* txt)
{
	unsigned len;

	sscanf(txt, "%u", &len);

	set_payload_length(len);

	return false;
}

void Radio::txPkt()
{
	//yyy TODO
	log_printf("todo txPkt\r\n");
}

menuMode_e Radio::tx_ramp_write(unsigned sidx)
{
	sx126x_status_t ret;
	//yyy TODO
	/*tx_param_buf[1] = sidx;
	radio.xfer(OPCODE_SET_TX_PARAMS, 2, 0, tx_param_buf);*/
	tx_param_buf[1] = sidx;
	ret = sx126x_set_tx_params(transceiver, tx_param_buf[0], (sx126x_ramp_time_t)tx_param_buf[1]);
	if (ret != SX126X_STATUS_OK) {
		log_printf("%d = sx126x_set_tx_params()\r\n", ret);
		return MENUMODE_NONE;
	}
	return MENUMODE_REDRAW;
}

const char* Radio::tx_ramp_strs[] = {
	"10  ", // 0
	"20  ", // 1
	"40  ", // 2
	"80  ", // 3
	"200 ", // 4
	"800 ", // 5
	"1700", // 6
	"3400", // 7
	NULL
};

unsigned Radio::tx_ramp_read(bool fw)
{
	PwrCtrl_t PwrCtrl;
	/*PwrCtrl.octet = radio.readReg(REG_ADDR_PWR_CTRL, 1);
	tx_param_buf[1] = PwrCtrl.octet >> 5;
	return PwrCtrl.bits.ramp_time;*/
	//yyy TODO
	sx126x_status_t ret = sx126x_read_register(transceiver, REG_ADDR_PWR_CTRL, &PwrCtrl.octet, 1);
	if (ret != SX126X_STATUS_OK) {
		log_printf("%d = sx126x_read_register", ret);
		return false;
	}
	tx_param_buf[1] = PwrCtrl.octet >> 5;
	return PwrCtrl.bits.ramp_time;
}

void Radio::write_register(unsigned addr, unsigned val)
{
	//radio.writeReg(addr, val, 1);
	//yyy TODO
}

unsigned Radio::read_register(unsigned addr)
{
	//return radio.readReg(addr, 1);
	//yyy TODO
	return 0;
}

void Radio::tx_dbm_print()
{
	PwrCtrl_t PwrCtrl;
	PaCtrl1b_t PaCtrl1b;
	uint8_t v;
	sx126x_status_t ret = sx126x_read_register(transceiver, REG_ADDR_ANACTRL16, &v, 1);
	if (ret != SX126X_STATUS_OK) {
		log_printf("%d = sx126x_read_register", ret);
		return;
	}
	if (v & 0x10) {
		printf_uart("%d", PA_OFF_DBM);
		return;
	}
	ret = sx126x_read_register(transceiver, REG_ADDR_PWR_CTRL, &PwrCtrl.octet, 1);
	if (ret != SX126X_STATUS_OK) {
		log_printf("%d = sx126x_read_register", ret);
		return;
	}
	ret = sx126x_read_register(transceiver, REG_ADDR_PA_CTRL1B, &PaCtrl1b.octet, 1);
	if (ret != SX126X_STATUS_OK) {
		log_printf("%d = sx126x_read_register", ret);
		return;
	}
	pa_config_buf[2] = PaCtrl1b.bits.tx_mode_bat; // deviceSel

	if (PaCtrl1b.bits.tx_mode_bat)
		printf_uart("%d", PwrCtrl.bits.tx_pwr - 17);
	else
		printf_uart("%d", PwrCtrl.bits.tx_pwr - 9);
#if 0
	PwrCtrl_t PwrCtrl;
	PaCtrl1b_t PaCtrl1b;
	unsigned v = radio.readReg(REG_ADDR_ANACTRL16, 1);

	if (v & 0x10) {
		printf_uart("%d", PA_OFF_DBM);
		return;
	}

	PwrCtrl.octet = radio.readReg(REG_ADDR_PWR_CTRL, 1);

	PaCtrl1b.octet = radio.readReg(REG_ADDR_PA_CTRL1B, 1);
	pa_config_buf[2] = PaCtrl1b.bits.tx_mode_bat; // deviceSel

	if (PaCtrl1b.bits.tx_mode_bat)
		printf_uart("%d", PwrCtrl.bits.tx_pwr - 17);
	else
		printf_uart("%d", PwrCtrl.bits.tx_pwr - 9);
#endif /* #if 0 */
	//yyy TODO
}

bool Radio::tx_dbm_write(const char* str)
{
#if 0
	int dbm;
	unsigned v = radio.readReg(REG_ADDR_ANACTRL16, 1);

	sscanf(str, "%d", &dbm);

	if (dbm == PA_OFF_DBM) {
		/* bench test: prevent overloading receiving station (very low tx power) */
		v |= 0x10; 	// pa dac atb tst
		radio.writeReg(REG_ADDR_ANACTRL16, v, 1);
	} else {
		tx_param_buf[0] = dbm;
		radio.xfer(OPCODE_SET_TX_PARAMS, 2, 0, tx_param_buf);

		if (v & 0x10) {
			v &= ~0x10;
			radio.writeReg(REG_ADDR_ANACTRL16, v, 1);
		}
	}

	return false;
#endif /* #if 0 */
	int dbm;
	uint8_t v;
	sx126x_status_t ret = sx126x_read_register(transceiver, REG_ADDR_ANACTRL16, &v, 1);
	if (ret != SX126X_STATUS_OK) {
		log_printf("%d = sx126x_read_register", ret);
		return false;
	}
	sscanf(str, "%d", &dbm);
	if (dbm == PA_OFF_DBM) {
		/* bench test: prevent overloading receiving station (very low tx power) */
		v |= 0x10; 	// pa dac atb tst
		ret = sx126x_write_register(transceiver, REG_ADDR_ANACTRL16, &v, 1);
		if (ret != SX126X_STATUS_OK) {
			log_printf("%d = sx126x_write_register", ret);
			return false;
		}
	} else {
		tx_param_buf[0] = dbm;
		sx126x_status_t ret = sx126x_set_tx_params(transceiver, tx_param_buf[0], (sx126x_ramp_time_t)tx_param_buf[1]);
		if (ret != SX126X_STATUS_OK) {
			log_printf("%d = sx126x_set_tx_params()\r\n", ret);
			return false;
		}
		if (v & 0x10) {
			v &= ~0x10;
			ret = sx126x_write_register(transceiver, REG_ADDR_ANACTRL16, &v, 1);
			if (ret != SX126X_STATUS_OK) {
				log_printf("%d = sx126x_write_register", ret);
				return false;
			}
		}
	}
	return false;
}

const menu_t* Radio::get_modem_sub_menu() { return NULL; }

const menu_t* Radio::get_modem_menu()
{
	sx126x_pkt_type_t pt;
	sx126x_status_t ret = sx126x_get_pkt_type(transceiver, &pt);
	if (ret != SX126X_STATUS_OK) {
		log_printf("%d = sx126x_get_pkt_type()\r\n", ret);
		return NULL;
	}
	//pktType = radio.getPacketType();

	if (pt == SX126X_PKT_TYPE_LORA) {
		return lora_menu;
	} else if (pt == SX126X_PKT_TYPE_GFSK) {
		return gfsk_menu;
	} else if (pt == SX126X_PKT_TYPE_BPSK) {
		log_printf("TODO BPSK menu\r\n");
		return NULL;
	} else if (pt == SX126X_PKT_TYPE_LR_FHSS) {
		log_printf("TODO LR_FHSS menu\r\n");
		return NULL;
	}

	return NULL;
}

float Radio::getMHz()
{
	uint8_t buf[4];
	uint32_t frf;
	sx126x_status_t ret;
	ret = sx126x_read_register(transceiver, REG_ADDR_RFFREQ, buf, 4);
	if (ret != SX126X_STATUS_OK) {
		log_printf("%d = sx126x_read_register", ret);
		return 0;
	}
	frf = buf[0];
	frf <<= 8;
	frf |= buf[1];
	frf <<= 8;
	frf |= buf[2];
	frf <<= 8;
	frf |= buf[3];
    return frf / (float)MHZ_TO_FRF;
}

void Radio::setMHz(float MHz)
{
	sx126x_status_t ret = sx126x_set_rf_freq(transceiver, MHz * 1000000);
	if (ret != SX126X_STATUS_OK) {
		log_printf("%d = sx126x_set_rf_freq", ret);
	}
	
}

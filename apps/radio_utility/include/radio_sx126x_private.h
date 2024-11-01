
        static bool deviceSel_read(void);
        static bool deviceSel_push(void);
        static const toggle_item_t deviceSel_item;

        static unsigned tcxoDelayTicks; // runs at 64,000Hz
        static void tcxo_delay_print(void);
        static bool tcxo_delay_write(const char*);
        static const value_item_t tcxo_delay_item;

        static unsigned paDutyCycle_read(bool);
        static menuMode_e paDutyCycle_write(unsigned sidx);
        static const dropdown_item_t paDutyCycle_item;

        static void xta_print(void);
        static bool xta_write(const char*);
        static const value_item_t xta_item;

        static void xtb_print(void);
        static bool xtb_write(const char*);
        static const value_item_t xtb_item;

        static void tcxo_volts_print(void);
        static bool tcxo_volts_write(const char*);
        static const value_item_t tcxo_volts_item;

        static unsigned rxfe_pm_read(bool);
        static menuMode_e rxfe_pm_write(unsigned sidx);
        static const dropdown_item_t rxfe_pm_item;

        static unsigned hpMax_read(bool);
        static menuMode_e hpMax_write(unsigned sidx);
        static const dropdown_item_t hpMax_item;

        static void ldo_push(void);
        static const button_item_t ldo_item;

        static void ocp_print(void);
        static bool ocp_write(const char*);
        static const value_item_t ocp_item;

        static void dcdc_push(void);
        static const button_item_t dcdc_item;

        /**************** lora: ************************/

        static unsigned lora_bw_read(bool);
        static menuMode_e lora_bw_write(unsigned sidx);
        static const dropdown_item_t lora_bw_item;

        static bool lora_inviq_read(void);
        static bool lora_inviq_push(void);
        static const toggle_item_t lora_inviq_item;

        static void lora_sf_print(void);
        static bool lora_sf_write(const char*);
        static const value_item_t lora_sf_item;

        static unsigned lora_cr_read(bool);
        static menuMode_e lora_cr_write(unsigned sidx);
        static const dropdown_item_t lora_cr_item;

        static bool lora_crcon_read(void);
        static bool lora_crcon_push(void);
        static const toggle_item_t lora_crcon_item;

        static void cad_push(void);
        static const button_item_t lora_cad_item;

        static bool lora_cadexit_read(void);
        static bool lora_cadexit_push(void);
        static const toggle_item_t lora_cadexit_item;

        static bool lora_sdmode_read(void);  
        static bool lora_sdmode_push(void);
        static const toggle_item_t lora_sdmode_item;

        static bool ppmOffset_read(void);
        static bool ppmOffset_push(void);
        static const toggle_item_t lora_ppmOffset_item;

        static void lora_pblLen_print(void);
        static bool lora_pblLen_write(const char*);
        static const value_item_t lora_pblLen_item;

        static void lora_cadpnratio_print(void);
        static bool lora_cadpnratio_write(const char*);
        static const value_item_t lora_cadpnratio_item;

        static bool lora_headerType_read(void);
        static bool lora_headerType_push(void);
        static const toggle_item_t lora_headerType_item;

        static void lora_ppg_print(void);
        static bool lora_ppg_write(const char*);
        static const value_item_t lora_ppg_item;
        static uint16_t ppg;

        static unsigned lora_cadsymbs_read(bool);
        static menuMode_e lora_cadsymbs_write(unsigned sidx);
        static const dropdown_item_t lora_cadsymbs_item;

        static void lora_cadtimeout_print(void);
        static bool lora_cadtimeout_write(const char*);
        static const value_item_t lora_cadtimeout_item;

        static void lora_cadmin_print(void);
        static bool lora_cadmin_write(const char*);
        static const value_item_t lora_cadmin_item;

		static uint8_t bw_idx;

        /**************** gfsk: ************************/

        static void gfsk_bitrate_print(void);
        static bool gfsk_bitrate_write(const char*);
        static const value_item_t gfsk_bitrate_item;

		static void gfsk_whiteInit_print(void);
        static bool gfsk_whiteInit_write(const char*);
        static const value_item_t gfsk_whiteInit_item;

        static bool gfsk_white_read(void);
        static bool gfsk_white_push(void);
        static const toggle_item_t gfsk_white_item;

        static void gfsk_syncword_print(void);
        static bool gfsk_syncword_write(const char*);
        static const value_item_t gfsk_syncword_item;

        static void gfsk_crcpoly_print(void);
        static bool gfsk_crcpoly_write(const char*);
        static const value_item_t gfsk_crcpoly_item;

        static unsigned gfsk_addrcomp_read(bool);
        static menuMode_e gfsk_addrcomp_write(unsigned sidx);
        static const dropdown_item_t gfsk_addrcomp_item;

        static void gfsk_fdev_print(void);
        static bool gfsk_fdev_write(const char*);
        static const value_item_t gfsk_fdev_item;	

        static void gfsk_crcinit_print(void);
        static bool gfsk_crcinit_write(const char*);
        static const value_item_t gfsk_crcinit_item;

        static void gfsk_nodeadrs_print(void);
        static bool gfsk_nodeadrs_write(const char*);
        static const value_item_t gfsk_nodeadrs_item;

        static unsigned gfsk_pblDetLen_read(bool);
        static menuMode_e gfsk_pblDetLen_write(unsigned sidx);
        static const dropdown_item_t gfsk_pblDetLen_item;

        static unsigned gfsk_bt_read(bool);
        static menuMode_e gfsk_bt_write(unsigned sidx);
        static const dropdown_item_t gfsk_bt_item;

        static unsigned gfsk_crctype_read(bool);
        static menuMode_e gfsk_crctype_write(unsigned sidx);
        static const dropdown_item_t gfsk_crctype_item;

        static void gfsk_broadcast_print(void);
        static bool gfsk_broadcast_write(const char*);
        static const value_item_t gfsk_broadcast_item;

        static void gfsk_pblLen_print(void);
        static bool gfsk_pblLen_write(const char*);
        static const value_item_t gfsk_pblLen_item;

        static unsigned gfsk_rxbw_read(bool);
        static menuMode_e gfsk_rxbw_write(unsigned sidx);
        static const dropdown_item_t gfsk_rxbw_item;

        static void gfsk_swl_print(void);
        static bool gfsk_swl_write(const char*);
        static const value_item_t gfsk_swl_item;

        static bool gfsk_fixLen_read(void);
        static bool gfsk_fixLen_push(void);
        static const toggle_item_t gfsk_fixLen_item;

        static uint8_t cadParams[7];

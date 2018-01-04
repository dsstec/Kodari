/*
 * kodari-micom-core.h
 *
 *
 */

#ifndef __LINUX_MFD_KODARI_MICOM_CORE_H
#define __LINUX_MFD_KODARI_MICOM_CORE_H

#define KODARI_MICOM_NUM_IRQ_REGS	1

enum kodari_micom_device_type {
	KODARI_BATTERY,
	KODARI_BMODULE,
};

enum kodari_micom_regs {
	KODARI_AC_ONLINE_0,
	KODARI_BATTERY_ONLINE_0,
	KODARI_VCELL_MSB_0,
	KODARI_VCELL_LSB_0,
	KODARI_SOC_MSB_0,
	KODARI_AAA_BATTERY_ONLINE_0, //KODARI_SOC_LSB_0,
	KODARI_MICOM_MAJOR_VER,
	KODARI_MICOM_MINOR_VER,
	KODARI_BOARD_VER,
	KODARI_BMODULE_LED_CONTROL, // Control bmodule_led
	KODARI_BMODULE_MONITOR_START, // Notify bmodule_monitor_start (MCU is send bmodule_status_info after this command received)
	KODARI_SLEEP_MODE_ENTER_NOTIFY, // Notify sleep_mode_enter
	KODARI_BMODULE_POWER_CONTROL,
	//KODARI_EMERGENCY_ERASE_REASON_REQUEST = 0xF, // Ask emergency_erase_reason
    KODARI_BATTERY_LED_DISABLE = 0xF, // Disable battery led
	KODARI_AC_ONLINE_1 =		0x10,
	KODARI_BATTERY_ONLINE_1,
	KODARI_VCELL_MSB_1,
	KODARI_VCELL_LSB_1,
	KODARI_SOC_MSB_1,
	KODARI_AAA_BATTERY_ONLINE_1, //KODARI_SOC_LSB_1,
	KODARI_BATTERY_LED_CONTROL, // Notify received-message, received-message read done
	KODARI_BMODULE_STATES = 0x20,
	KODARI_DC_ADAPTER_ONLINE = 0x21,
	KODARI_BMODULE_ERASE_NOTIFY = 0x30, // Notify bmodule_erase (whether erase is true or not)
};


#define KODARI_BATTERY_TYPE_MOBILE	0xAA
#define KODARI_BATTERY_TYPE_CAR		0x55

enum kodari_micom_irq {
	KODARI_MICOM_BMODULE_ENC_PWR_ON,
	KODARI_MICOM_BMODULE_ENC_ERASE_OK,
	KODARI_MICOM_BMODULE_ENC_FAIL,
	KODARI_MICOM_BMODULE_ENC_INJECT_OK,
	KODARI_MICOM_BMODULE_ENC_INJECT_ING,
	KODARI_MICOM_BMODULE_ENC_INJECT_RST,
	KODARI_MICOM_BMODULE_ENC_INJECT_BOOT_OK,
	KODARI_MICOM_BMODULE_ENC_INJECT_DETECT,
	KODARI_MICOM_IRQ_NR,
};




#define KODARI_MICOM_BMODULE_ENC_PWR_ON_MASK				(1 << 7)
#define KODARI_MICOM_BMODULE_ENC_ERASE_OK_MASK				(1 << 6)
#define KODARI_MICOM_BMODULE_ENC_FAIL_MASK					(1 << 5)
#define KODARI_MICOM_BMODULE_ENC_INJECT_OK_MASK			(1 << 4)
#define KODARI_MICOM_BMODULE_ENC_INJECT_ING_MASK			(1 << 3)
#define KODARI_MICOM_BMODULE_ENC_INJECT_RST_MASK			(1 << 2)
#define KODARI_MICOM_BMODULE_ENC_INJECT_BOOT_OK_MASK		(1 << 1)
#define KODARI_MICOM_BMODULE_ENC_INJECT_DETECT_MASK		(1 << 0)

struct kodari_micom_dev {
	struct device *dev;
	struct i2c_client *i2c;
	void *kodari_battery_dev;
	
	int irq_base;
	struct mutex iolock;
	struct mutex irqlock;

	u8 irq_masks_cur;	
	u8 irq_masks_cache;

	int irq;
	int ono;
	bool wakeup;
};


int kodari_micom_irq_init(struct kodari_micom_dev *kodaridev);
void kodari_micom_irq_exit(struct kodari_micom_dev *kodaridev);
int kodari_micom_irq_resume(struct kodari_micom_dev *kodaridev);

extern int kodari_micom_reg_read(struct i2c_client *i2c, u8 reg, u8 *dest);
extern int kodari_micom_reg_write(struct i2c_client *i2c, u8 reg, u8 value);
extern int kodari_micom_reg_update(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);
extern int bmodule_detect_status;
extern int bmodule_probe_done;

struct kodari_micom_platform_data {
		int (*battery_online)(void);
	int (*charger_online)(void);
	int (*charger_enable)(void);

	int				irq_base;
	int (*cfg_kodari_micom_irq)(void);
	int				ono;
	bool				wakeup;
	
};

#endif /*  */

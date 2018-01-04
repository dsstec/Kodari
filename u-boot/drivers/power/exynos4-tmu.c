
#include <common.h>
#include <errno.h>
#include <asm/arch/exynos4-tmu.h>
#include <asm/arch/power.h>
#include <asm/arch/cpu.h>


#define TRIMINFO_RELOAD		1
#define CORE_EN			1
#define THERM_TRIP_EN		(1 << 12)
#define HW_TRIP_MODE		(7<<13)


#define INTEN_RISE0		1
#define INTEN_RISE1		(1 << 4)
#define INTEN_RISE2		(1 << 8)
#define INTEN_FALL0		(1 << 16)
#define INTEN_FALL1		(1 << 20)
#define INTEN_FALL2		(1 << 24)

#define TRIM_INFO_MASK		0xff

#define INTCLEAR_RISE0		1
#define INTCLEAR_RISE1		(1 << 4)
#define INTCLEAR_RISE2		(1 << 8)
#define INTCLEAR_FALL0		(1 << 16)
#define INTCLEAR_FALL1		(1 << 20)
#define INTCLEAR_FALL2		(1 << 24)
#define INTCLEARALL		(INTCLEAR_RISE0 | INTCLEAR_RISE1 | \
				 INTCLEAR_RISE2 | INTCLEAR_FALL0 | \
				 INTCLEAR_FALL1 | INTCLEAR_FALL2)

/* Tmeperature threshold values for various thermal events */
struct temperature_params {
	/* minimum value in temperature code range */
	unsigned int min_val;
	/* maximum value in temperature code range */
	unsigned int max_val;
	/* temperature threshold to start warning */
	unsigned int start_warning;
	/* temperature threshold CPU tripping */
	unsigned int start_tripping;
	/* temperature threshold for HW tripping */
	unsigned int hardware_tripping;
	unsigned int start_throttle;
};

/* Pre-defined values and thresholds for calibration of current temperature */
struct tmu_data {
	/* pre-defined temperature thresholds */
	struct temperature_params ts;
	/* pre-defined efuse range minimum value */
	unsigned int efuse_min_value;
	/* pre-defined efuse value for temperature calibration */
	unsigned int efuse_value;
	/* pre-defined efuse range maximum value */
	unsigned int efuse_max_value;
	/* current temperature sensing slope */
	unsigned int slope;
};

/* TMU device specific details and status */
struct tmu_info {
	/* base Address for the TMU */
//	unsigned int tmu_base;
	/* mux Address for the TMU */
	int tmu_mux;
	/* pre-defined values for calibration and thresholds */
	struct tmu_data data;
	/* value required for triminfo_25 calibration */
	unsigned int te1;
	/* value required for triminfo_85 calibration */
	unsigned int te2;
	/* TMU DC value for threshold calculation */
	int dc_value;
	/* enum value indicating status of the TMU */
	int tmu_state;
};

/* Global struct tmu_info variable to store init values */
static struct tmu_info gbl_info;


/*
 * After reading temperature code from register, compensating
 * its value and calculating celsius temperatue,
 * get current temperatue.
 *
 * @return	current temperature of the chip as sensed by TMU
 */
int get_cur_temp(struct tmu_info *info)
{
	int cur_temp;
	int temperature;
	struct tmu_reg *reg = (struct tmu_reg *)samsung_get_base_tmu();

	/* Temperature code range between min 25 and max 125 */
	//cur_temp = readl(&reg->current_temp) & 0xff;

	cur_temp = CURRENT_TEMP_REG & 0xff;


	temperature = cur_temp - info->te1 + info->dc_value;

	/* Calibrate current temperature */
	if (temperature < 10)
		printf("Current temperature is in inaccurate range->"
			" check if vdd_18_ts is on or room temperature.\n");

	return temperature;
}

/*
 * Monitors status of the TMU device and exynos temperature
 *
 * @param temp	pointer to the current temperature value
 * @return	enum tmu_status_t value, code indicating event to execute
 */
enum tmu_status_t tmu_monitor(int *temp)
{


	if (gbl_info.tmu_state == TMU_STATUS_INIT)
		return -1;

	int cur_temp;
	struct tmu_data *data = &gbl_info.data;

	/* Read current temperature of the SOC */
	cur_temp = get_cur_temp(&gbl_info);
	if (!cur_temp)
		goto out;
	
	*temp = cur_temp;



	/* Temperature code lies between min 25 and max 125 */
	if (cur_temp >= data->ts.start_tripping &&
			cur_temp <= data->ts.max_val)
		return TMU_STATUS_TRIPPED;
	else if (cur_temp >= data->ts.start_warning)
		return TMU_STATUS_WARNING;
	else if (cur_temp < data->ts.start_warning &&
			cur_temp >= data->ts.min_val)
		return TMU_STATUS_NORMAL;
	/* Temperature code does not lie between min 25 and max 125 */
	else {
		gbl_info.tmu_state = TMU_STATUS_INIT;
		printf("EXYNOS_TMU: Thermal reading failed\n");
		return -1;
	}
out:
	/* Temperature code does not lie between min 25 and max 125 */
	gbl_info.tmu_state = TMU_STATUS_INIT;
	printf("EXYNOS_TMU: Thermal reading failed\n");
	return TMU_STATUS_INIT;

}

/*
 * Calibrate and calculate threshold values and
 * enable interrupt levels
 *
 * @param	info pointer to the tmu_info struct
 */
void tmu_setup_parameters(struct tmu_info *info)
{
	unsigned int te_temp, con;
	unsigned int warning_code, trip_code, hwtrip_code;
	unsigned int cooling_temp;
	unsigned int rising_value;
	unsigned int temp_throttle;
	struct tmu_data *data = &info->data;
	struct tmu_reg *reg = (struct tmu_reg *)samsung_get_base_tmu();

	/* Must reload for using efuse value at EXYNOS */
//	writel(TRIMINFO_RELOAD, &reg->triminfo_control);
	TRIMINFO_CONTROL_REG = TRIMINFO_RELOAD;

	

	/* Get the compensation parameter */
	te_temp = readl(&reg->triminfo);
	info->te1 = te_temp & TRIM_INFO_MASK;
	info->te2 = ((te_temp >> 8) & TRIM_INFO_MASK);

	if ((data->efuse_min_value > info->te1) ||
			(info->te1 > data->efuse_max_value)
			||  (info->te2 != 0))
		info->te1 = data->efuse_value;

	/* Get RISING & FALLING Threshold value */
	temp_throttle = data->ts.start_throttle
			+ info->te1 - info->dc_value;
	warning_code = data->ts.start_warning + info->te1 - info->dc_value;
	trip_code = data->ts.start_tripping + info->te1 - info->dc_value;
	hwtrip_code = data->ts.hardware_tripping + info->te1 - info->dc_value;

	cooling_temp = 0;

	rising_value = (temp_throttle | (warning_code << 8) |
			(trip_code << 16) |
			(hwtrip_code << 24));

	/* Set interrupt level */
	//writel(rising_value, &reg->threshold_temp_rise);
	THRESHOLD_TEMP_RISE_REG = rising_value;
	
	//writel(cooling_temp, &reg->threshold_temp_fall);
	THRESHOLD_TEMP_FALL_REG = cooling_temp;

	/*
	 * Need to init all register settings after getting parameter info
	 * [28:23] vref [11:8] slope - Tuning parameter
	 *
	 * WARNING: this slope value writes into many bits in the tmu_control
	 * register, with the default FDT value of 268470274 (0x10008802)
	 * we are using this essentially sets the default register setting
	 * from the TRM for tmu_control.
	 * TODO(bhthompson): rewrite this code such that we are not performing
	 * a hard wipe of tmu_control and re verify functionality.
	 */
	//writel(data->slope, &reg->tmu_control);
	TMU_CONTROL_REG = data->slope;

	//writel(INTCLEARALL, &reg->intclear);
	INTCLEAR_REG = INTCLEARALL;
	/* TMU core enable */
	//con = readl(&reg->tmu_control);
	printf("tmu control : %x\n", TMU_CONTROL_REG);
	con |= HW_TRIP_MODE;
	//TMU_CONTROL_REG |= HW_TRIP_MODE;
	
	//con |= (info->tmu_mux << 20) | THERM_TRIP_EN | CORE_EN;
	TMU_CONTROL_REG |=  (info->tmu_mux << 20) | THERM_TRIP_EN | CORE_EN;
	printf("tmu control : %x\n", TMU_CONTROL_REG);

	//writel(con, &reg->tmu_control);

	//con = readl(&reg->tmu_control);
	//printf("tmu control : %x\n", con);

	/* Enable HW thermal trip */
	power_enable_hw_thermal_trip();

	/* LEV1 LEV2 interrupt enable */
	//writel(INTEN_RISE0 | INTEN_RISE1 | INTEN_RISE2, &reg->inten);

	INTEN_REG = INTEN_RISE0 | INTEN_RISE1 | INTEN_RISE2;
}


void tmu_set_default_values(struct tmu_info *info)
{
	info->tmu_mux  = 6;
	info->data.ts.min_val = 25;
	info->data.ts.max_val = 125;
	info->data.ts.start_throttle = 90;
	info->data.ts.start_warning = 95;	
	info->data.ts.start_tripping = 105;
	info->data.ts.hardware_tripping = 110;
	info->data.efuse_min_value = 40;
	info->data.efuse_value = 55;
	info->data.efuse_max_value = 100;
	info->data.slope = 0x10008802;
	info->dc_value = 25;

	

	
}

/*
 * Initialize TMU device
 *
 * @param blob  FDT blob
 * @return	int value, 0 for success
 */
int tmu_init(void)
{
	printf("%s\n", __func__);
	tmu_set_default_values(&gbl_info);
	tmu_setup_parameters(&gbl_info);
	gbl_info.tmu_state = TMU_STATUS_NORMAL;

	return 0;
}


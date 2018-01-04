#include <common.h>
#include <asm/arch/clock.h>
#include <asm/arch/exynos4-cpufreq.h>
#include <asm/arch/pmic.h>
#include <asm/arch/cpu.h>



/* APLL CON0 */
#define CON0_LOCK_BIT_MASK	(0x1 << 29)
#define MDIV_MASK(x)		(x << 16)
#define PDIV_MASK(x)		(x << 8)
#define SDIV_MASK(x)		(x << 0)
#define APLL_PMS_MASK		 ~(MDIV_MASK(0x3ff)	\
				| PDIV_MASK(0x3f) | SDIV_MASK(0x7))
				

#define CPUFREQ_LEVEL_END	(CPU_FREQ_L200 + 1)

#undef PRINT_DIV_VAL

#define ENABLE_CLKOUT

static int max_support_idx;
static int min_support_idx = (CPUFREQ_LEVEL_END - 1);
static enum cpufreq_level old_freq_level;



struct cpufreq_clkdiv {
	unsigned int	index;
	unsigned int	clkdiv;
	unsigned int	clkdiv1;
};

extern void I2C_S5M8767_VolSetting(PMIC_RegNum eRegNum, unsigned char ucVolLevel, unsigned char ucEnable);


static struct cpufreq_clkdiv exynos4x12_clkdiv_table[CPUFREQ_LEVEL_END];


static unsigned int exynos4x12_volt_table[CPUFREQ_LEVEL_END];

/* CLK_DIV_CPU0_VAL */
#define DIV_CPU0_RSVD		~((0x7 << 28) | (0x7 << 24) | (0x7 << 20)	| (0x7 << 16) | (0x7 << 12) | (0x7 << 8) | (0x7 << 4)	| (0x7))

/* CLK_DIV_CPU1 */
#define DIV_CPU1_RSVD		~((0x7 << 8) | (0x7 << 4) | (0x7))



static unsigned int clkdiv_cpu0_4412[CPUFREQ_LEVEL_END][8] = {
	/*
	 * Clock divider value for following
	 * { DIVCORE, DIVCOREM0, DIVCOREM1, DIVPERIPH,
	 *		DIVATB, DIVPCLK_DBG, DIVAPLL, DIVCORE2 }
	 */
	/* ARM L0: 1600Mhz */
	{ 0, 3, 7, 0, 6, 1, 7, 0 },

	/* ARM L1: 1500Mhz */
	{ 0, 3, 7, 0, 6, 1, 7, 0 },

	/* ARM L2: 1400Mhz */
	{ 0, 3, 7, 0, 6, 1, 6, 0 },

	/* ARM L3: 1300Mhz */
	{ 0, 3, 7, 0, 5, 1, 6, 0 },

	/* ARM L4: 1200Mhz */
	{ 0, 3, 7, 0, 5, 1, 5, 0 },

	/* ARM L5: 1100MHz */
	{ 0, 3, 6, 0, 4, 1, 5, 0 },

	/* ARM L6: 1000MHz */
	{ 0, 2, 5, 0, 4, 1, 4, 0 },

	/* ARM L7: 900MHz */
	{ 0, 2, 5, 0, 3, 1, 4, 0 },

	/* ARM L8: 800MHz */
	{ 0, 2, 5, 0, 3, 1, 3, 0 },

	/* ARM L9: 700MHz */
	{ 0, 2, 4, 0, 3, 1, 3, 0 },

	/* ARM L10: 600MHz */
	{ 0, 2, 4, 0, 3, 1, 2, 0 },

	/* ARM L11: 500MHz */
	{ 0, 2, 4, 0, 3, 1, 2, 0 },

	/* ARM L12: 400MHz */
	{ 0, 2, 4, 0, 3, 1, 1, 0 },

	/* ARM L13: 300MHz */
	{ 0, 2, 4, 0, 2, 1, 1, 0 },

	/* ARM L14: 200MHz */
	{ 0, 1, 3, 0, 1, 1, 1, 0 },
};

static unsigned int clkdiv_cpu1_4412[CPUFREQ_LEVEL_END][3] = {
	/* Clock divider value for following
	 * { DIVCOPY, DIVHPM, DIVCORES }
	 */
	/* ARM L0: 1600MHz */
	{ 6, 0, 7 },

	/* ARM L1: 1500MHz */
	{ 6, 0, 7 },

	/* ARM L2: 1400MHz */
	{ 6, 0, 6 },

	/* ARM L3: 1300MHz */
	{ 5, 0, 6 },

	/* ARM L4: 1200MHz */
	{ 5, 0, 5 },

	/* ARM L5: 1100MHz */
	{ 4, 0, 5 },

	/* ARM L6: 1000MHz */
	{ 4, 0, 4 },

	/* ARM L7: 900MHz */
	{ 3, 0, 4 },

	/* ARM L8: 800MHz */
	{ 3, 0, 3 },

	/* ARM L9: 700MHz */
	{ 3, 0, 3 },

	/* ARM L10: 600MHz */
	{ 3, 0, 2 },

	/* ARM L11: 500MHz */
	{ 3, 0, 2 },

	/* ARM L12: 400MHz */
	{ 3, 0, 1 },

	/* ARM L13: 300MHz */
	{ 3, 0, 1 },

	/* ARM L14: 200MHz */
	{ 3, 0, 0 },
};




static struct cpufreq_frequency_table exynos4x12_freq_table[] = {
	{CPU_FREQ_L1600, 1600*1000},
	{CPU_FREQ_L1500, 1500*1000},
	{CPU_FREQ_L1400, 1400*1000},
	{CPU_FREQ_L1300, 1300*1000},
	{CPU_FREQ_L1200, 1200*1000},
	{CPU_FREQ_L1100, 1100*1000},
	{CPU_FREQ_L1000, 1000*1000},
	{CPU_FREQ_L900, 900*1000},
	{CPU_FREQ_L800, 800*1000},
	{CPU_FREQ_L700, 700*1000},
	{CPU_FREQ_L600, 600*1000},
	{CPU_FREQ_L500, 500*1000},
	{CPU_FREQ_L400, 400*1000},
	{CPU_FREQ_L300, 300*1000},
	{CPU_FREQ_L200, 200*1000},
	{0, CPU_FREQ_LCOUNT},
};

/* ASV table for 12.5mV step */
static const unsigned int asv_voltage_step_12_5_rev2[CPUFREQ_LEVEL_END][13] = {
	/*   ASV0,    ASV1,    ASV2,    ASV3,    ASV4,    ASV5,    ASV6,    ASV7,    ASV8,    ASV9,   ASV10,   ASV11, ASV12 */
	{1312500, 1312500, 1312500,	1312500, 1300000, 1287500, 1275000, 1262500, 1250000, 1237500, 1212500, 1200000, 1187500},/*L0*/
	{1275000, 1262500, 1262500,	1262500, 1250000, 1237500, 1225000, 1212500, 1200000, 1187500, 1162500, 1150000, 1137500},/*L1*/
	{1237500, 1225000, 1225000,	1225000, 1212500, 1200000, 1187500, 1175000, 1162500, 1150000, 1125000, 1112500, 1100000},/*L2*/
	{1187500, 1175000, 1175000,	1175000, 1162500, 1150000, 1137500, 1125000, 1112500, 1100000, 1075000, 1062500, 1050000},/*L3*/
	{1150000, 1137500, 1137500,	1137500, 1125000, 1112500, 1100000, 1087500, 1075000, 1062500, 1037500, 1025000, 1012500},/*L4*/
	{1112500, 1100000, 1100000,	1100000, 1087500, 1075000, 1062500, 1050000, 1037500, 1025000, 1000000,  987500,  975000},/*L5*/
	{1087500, 1075000, 1075000,	1075000, 1062500, 1050000, 1037500, 1025000, 1012500, 1000000,  975000,  962500,  950000},/*L6*/
	{1062500, 1050000, 1050000,	1050000, 1037500, 1025000, 1012500, 1000000,  987500,  975000,  950000,  937500,  925000},/*L7*/
	{1025000, 1012500, 1012500,	1012500, 1000000,  987500,  975000,  962500,  950000,  937500,  912500,  900000,  887500},/*L8*/
	{1000000,  987500,  987500,	 987500,  975000,  962500,  950000,  937500,  925000,  912500,  887500,  887500,  887500},/*L9*/
	{ 975000,  962500,  962500,	 962500,  950000,  937500,  925000,  912500,  900000,  887500,  875000,  875000,  875000},/*L10*/
	{ 962500,  950000,  950000,	 950000,  937500,  925000,  912500,  900000,  887500,  887500,  875000,  875000,  875000},/*L11*/
	{ 950000,  937500,  937500,	 937500,  925000,  912500,  900000,  887500,  887500,  887500,  875000,  875000,  875000},/*L12*/
	{ 937500,  925000,  925000,	 925000,  912500,  900000,  887500,  887500,  887500,  887500,  875000,  875000,  875000},/*L13*/
	{ 925000,  912500,  912500,	 912500,  900000,  887500,  887500,  887500,  887500,  887500,  875000,  875000,  875000},/*L14*/
};



static unsigned int exynos4x12_apll_pms_table[CPUFREQ_LEVEL_END] = {
	/* APLL FOUT L0: 1600MHz */
	((200<<16)|(3<<8)|(0x0)),

	/* APLL FOUT L1: 1500MHz */
	((250<<16)|(4<<8)|(0x0)),

	/* APLL FOUT L2: 1400MHz */
	((175<<16)|(3<<8)|(0x0)),

	/* APLL FOUT L3: 1300MHz */
	((325<<16)|(6<<8)|(0x0)),

	/* APLL FOUT L4: 1200MHz */
	((200<<16)|(4<<8)|(0x0)),

	/* APLL FOUT L5: 1100MHz */
	((275<<16)|(6<<8)|(0x0)),

	/* APLL FOUT L6: 1000MHz */
	((125<<16)|(3<<8)|(0x0)),

	/* APLL FOUT L7: 900MHz */
	((150<<16)|(4<<8)|(0x0)),

	/* APLL FOUT L8: 800MHz */
	((100<<16)|(3<<8)|(0x0)),

	/* APLL FOUT L9: 700MHz */
	((175<<16)|(3<<8)|(0x1)),

	/* APLL FOUT L10: 600MHz */
	((200<<16)|(4<<8)|(0x1)),

	/* APLL FOUT L11: 500MHz */
	((125<<16)|(3<<8)|(0x1)),

	/* APLL FOUT L12 400MHz */
	((100<<16)|(3<<8)|(0x1)),

	/* APLL FOUT L13: 300MHz */
	((200<<16)|(4<<8)|(0x2)),

	/* APLL FOUT L14: 200MHz */
	((100<<16)|(3<<8)|(0x2)),

};


int exynos4x12_pms_change(unsigned int old_index, unsigned int new_index)
{
	unsigned int old_pm = (exynos4x12_apll_pms_table[old_index] >> 8);
	unsigned int new_pm = (exynos4x12_apll_pms_table[new_index] >> 8);

	return (old_pm == new_pm) ? 0 : 1;
}


static void set_clkdiv(unsigned int div_index)
{
	unsigned int tmp;
	unsigned int stat_cpu1;
	struct exynos4_clock *clk =
		(struct exynos4_clock *)samsung_get_base_clock();

	/* Change Divider - CPU0 */

	tmp = exynos4x12_clkdiv_table[div_index].clkdiv;
//	__raw_writel(tmp, EXYNOS4_CLKDIV_CPU);
	writel(tmp,&clk->div_cpu0);

	do {
//		tmp = __raw_readl(EXYNOS4_CLKDIV_STATCPU);
		tmp = readl(&clk->div_stat_cpu0);
	} while (tmp & 0x11111111);


	//tmp = __raw_readl(EXYNOS4_CLKDIV_CPU);
	tmp = readl(&clk->div_cpu0);
	debug("DIV_CPU0[0x%x]\n", tmp);



	/* Change Divider - CPU1 */
	tmp = exynos4x12_clkdiv_table[div_index].clkdiv1;

	//__raw_writel(tmp, EXYNOS4_CLKDIV_CPU1);
	writel(tmp,&clk->div_cpu1);
	stat_cpu1 = 0x111;

	do {
//		tmp = __raw_readl(EXYNOS4_CLKDIV_STATCPU1);
		tmp = readl(&clk->div_stat_cpu1);
	} while (tmp & stat_cpu1);

	//tmp = __raw_readl(EXYNOS4_CLKDIV_CPU1);
	tmp = readl(&clk->div_cpu1);
	debug("DIV_CPU1[0x%x]\n", tmp);

}


#define EXYNOS4_CLKSRC_CPU_MUXCORE_SHIFT	(16)
#define EXYNOS4_CLKSRC_CPU_MUXCORE_SEL	(16)


static void set_apll(unsigned int new_index)
{
	unsigned int tmp, pdiv;	

	struct exynos4_clock *clk =
			(struct exynos4_clock *)samsung_get_base_clock();

	/* 1. MUX_CORE_SEL = MPLL,
	 * ARMCLK uses MPLL for lock time */
	tmp =  readl(&clk->src_cpu);
	tmp |= (0x1 << EXYNOS4_CLKSRC_CPU_MUXCORE_SEL);
	writel(tmp, &clk->src_cpu);	
	do {
		tmp = (readl(&clk->mux_stat_cpu) >> EXYNOS4_CLKSRC_CPU_MUXCORE_SHIFT);
		tmp &= 0x7;
	}while (tmp != 0x2);



	/* 2. Set APLL Lock time */
	pdiv = ((exynos4x12_apll_pms_table[new_index] >> 8) & 0x3f);
	//__raw_writel((pdiv * 250), EXYNOS4_APLL_LOCK);
	writel((pdiv * 250), &clk->apll_lock);

	/* 3. Change PLL PMS values */
	//tmp = __raw_readl(EXYNOS4_APLL_CON0);
	tmp = readl(&clk->apll_con0);
	tmp &= ~((0x3ff << 16) | (0x3f << 8) | (0x7 << 0));
	tmp |= exynos4x12_apll_pms_table[new_index];
	//__raw_writel(tmp, EXYNOS4_APLL_CON0);
	writel(tmp, &clk->apll_con0);

	/* 4. wait_lock_time */
	do {
		//tmp = __raw_readl(EXYNOS4_APLL_CON0);
		tmp = readl(&clk->apll_con0);
	} while (!(tmp & CON0_LOCK_BIT_MASK));


	/* 5. MUX_CORE_SEL = APLL */
	tmp =  readl(&clk->src_cpu);	
	tmp &= ~(0x1 << EXYNOS4_CLKSRC_CPU_MUXCORE_SEL);
	writel(tmp, &clk->src_cpu);		
	do {
		tmp = (readl(&clk->mux_stat_cpu)
			>> EXYNOS4_CLKSRC_CPU_MUXCORE_SHIFT);
		tmp &= 0x7;
	} while (tmp != 0x1);

}



#define CALC_S5M8767_BUCK234_VOLT(x)  ( (x<600000) ? 0 : ((x-600000)/6250) )
#define CALC_S5M8767_BUCK156_VOLT(x)  ( (x<650000) ? 0 : ((x-650000)/6250) )

/*
 * Switch ARM power corresponding to new frequency level
 *
 * @param new_volt_index	enum cpufreq_level, states new frequency
 * @return			int value, 0 for success
 */
static void exynos4x12_set_voltage(enum cpufreq_level new_volt_index)
{
	int error;
	unsigned int vdd_arm;
	I2C_S5M8767_VolSetting(PMIC_BUCK2, CALC_S5M8767_BUCK234_VOLT(exynos4x12_volt_table[new_volt_index]), 1);
	
	debug("voltage update corresponding to ARM frequency (%x)\n", 
		CALC_S5M8767_BUCK234_VOLT(exynos4x12_volt_table[new_volt_index]));

}



void exynos4x12_set_frequency(enum cpufreq_level new_freq_level)
{
	unsigned int tmp;
	
	struct exynos4_clock *clk =
			(struct exynos4_clock *)samsung_get_base_clock();

	


	if (old_freq_level > new_freq_level) {		
		exynos4x12_set_voltage(new_freq_level);
		if (!exynos4x12_pms_change(old_freq_level, new_freq_level)) {
			/* 1. Change the system clock divider values */
			set_clkdiv(new_freq_level);
			/* 2. Change just s value in apll m,p,s value */
			tmp = readl(&clk->apll_con0);
			tmp &= ~(0x7 << 0);
			tmp |= (exynos4x12_apll_pms_table[new_freq_level] & 0x7);
			writel(tmp, &clk->apll_con0);
		} else {
			set_clkdiv(new_freq_level);		
			set_apll(new_freq_level);				
		}
	}
	else if (old_freq_level < new_freq_level) {
		if (!exynos4x12_pms_change(old_freq_level, new_freq_level)) {
			/* 1. Change just s value in apll m,p,s value */
			tmp = readl(&clk->apll_con0);
			tmp &= ~(0x7 << 0);
			tmp |= (exynos4x12_apll_pms_table[new_freq_level] & 0x7);
			writel(tmp, &clk->apll_con0);
			/* 2. Change the system clock divider values */
			set_clkdiv(new_freq_level);

		}else {
			set_apll(new_freq_level);		
			set_clkdiv(new_freq_level);			
		}
		exynos4x12_set_voltage(new_freq_level);		
	}



	old_freq_level = new_freq_level;
	//printf("ARM Frequency changed %i\n", old_freq_level);
	//printf("APLL = %ldMHz, MPLL = %ldMHz\n", get_APLL_CLK()/1000000, get_MPLL_CLK()/1000000);

	
}


static void set_volt_table(void)
{
	unsigned int i;
	unsigned int exynos_result_of_asv = 1;

	max_support_idx = CPU_FREQ_L1600;


	//printf("DVFS : VDD_ARM Voltage table set with %d Group\n", exynos_result_of_asv);

	
	for (i = 0 ; i < CPUFREQ_LEVEL_END ; i++) {
		exynos4x12_volt_table[i] =
		asv_voltage_step_12_5_rev2[i][exynos_result_of_asv];
	}
	
		

}



int exynos4x12_cpufreq_init(void)
{
	int i;
	unsigned int tmp;
	struct exynos4_clock *clk =
		(struct exynos4_clock *)samsung_get_base_clock();

	set_volt_table();

	for (i = CPU_FREQ_L1600; i <  CPUFREQ_LEVEL_END; i++) {
		
		exynos4x12_clkdiv_table[i].index = i;
		tmp = readl(&clk->div_cpu0);		
		tmp &= DIV_CPU0_RSVD;
		
		tmp |= ((clkdiv_cpu0_4412[i][0] << 0) |
			(clkdiv_cpu0_4412[i][1] << 4) |
			(clkdiv_cpu0_4412[i][2] << 8) |
			(clkdiv_cpu0_4412[i][3] << 12) |
			(clkdiv_cpu0_4412[i][4] << 16) |
			(clkdiv_cpu0_4412[i][5] << 20) |
			(clkdiv_cpu0_4412[i][6] << 24) |
			(clkdiv_cpu0_4412[i][7] << 27));

		exynos4x12_clkdiv_table[i].clkdiv = tmp;
		
		tmp = readl(&clk->div_cpu1);
		tmp &= DIV_CPU1_RSVD;
		tmp |= ((clkdiv_cpu1_4412[i][0] << 0) |
			(clkdiv_cpu1_4412[i][1] << 4) |
			(clkdiv_cpu1_4412[i][2] << 8));
		exynos4x12_clkdiv_table[i].clkdiv1 = tmp;
		
	}

	old_freq_level = CPU_FREQ_L800;

	return 0;
}

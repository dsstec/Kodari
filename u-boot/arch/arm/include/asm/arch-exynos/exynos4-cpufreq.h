/* Define various levels of ARM frequency */
enum cpufreq_level {
	CPU_FREQ_L1600,		/* 1600 MHz */
	CPU_FREQ_L1500, 	/* 1500 MHz */
	CPU_FREQ_L1400, 	/* 1400 MHz */
	CPU_FREQ_L1300, 	/* 1300 MHz */
	CPU_FREQ_L1200, 	/* 1200 MHz */
	CPU_FREQ_L1100, 	/* 1100 MHz */
	CPU_FREQ_L1000, 	/* 1000 MHz */
	CPU_FREQ_L900,		/* 900 MHz */
	CPU_FREQ_L800,		/* 800 MHz */
	CPU_FREQ_L700,		/* 700 MHz */
	CPU_FREQ_L600,		/* 600 MHz */
	CPU_FREQ_L500,		/* 500 MHz */
	CPU_FREQ_L400,		/* 400 MHz */
	CPU_FREQ_L300,		/* 300 MHz */
	CPU_FREQ_L200,		/* 200 MHz */
	CPU_FREQ_LCOUNT,
};

struct cpufreq_frequency_table {
	unsigned int	index;     /* any */
	unsigned int	frequency; /* kHz - doesn't need to be in ascending
				    * order */
};

/*
 * Initialize ARM frequency scaling
 *
 *
 */
int exynos4x12_cpufreq_init(void);

/*
 * Switch ARM frequency to new level
 *
 * @param new_freq_level	enum cpufreq_level, states new frequency
 * @return			int value, 0 for success
 */
void exynos4x12_set_frequency(enum cpufreq_level new_freq_level);


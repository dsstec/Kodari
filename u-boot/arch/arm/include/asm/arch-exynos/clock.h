/*
 * (C) Copyright 2009 Samsung Electronics
 * Minkyu Kang <mk7.kang@samsung.com>
 * Heungjun Kim <riverful.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 */

#ifndef __ASM_ARM_ARCH_CLOCK_H_
#define __ASM_ARM_ARCH_CLOCK_H_

#ifndef __ASSEMBLY__

struct exynos4_clock {
	unsigned char	res1[0x4200];
	unsigned int	src_leftbus;
	unsigned char	res2[0x1fc];
	unsigned int	mux_stat_leftbus;
	unsigned char	res3[0xfc];
	unsigned int	div_leftbus;
	unsigned char	res4[0xfc];
	unsigned int	div_stat_leftbus;
	unsigned char	res5[0x1fc];
	unsigned int	gate_ip_leftbus;
	unsigned char	res6[0x12c];
	unsigned int	gate_ip_image;
	unsigned char	res7[0xcc];
	unsigned int	clkout_leftbus;
	unsigned int	clkout_leftbus_div_stat;
	unsigned char	res8[0x37f8];
	unsigned int	src_rightbus;
	unsigned char	res9[0x1fc];
	unsigned int	mux_stat_rightbus;
	unsigned char	res10[0xfc];
	unsigned int	div_rightbus;
	unsigned char	res11[0xfc];
	unsigned int	div_stat_rightbus;
	unsigned char	res12[0x1fc];
	unsigned int	gate_ip_rightbus;
	unsigned char	res13[0x15c];
	unsigned int	gate_ip_perir;
	unsigned char	res14[0x9c];
	unsigned int	clkout_rightbus;
	unsigned int	clkout_rightbus_div_stat;
	unsigned char	res15[0x3608];
	unsigned int	epll_lock;
	unsigned char	res16[0xc];
	unsigned int	vpll_lock;
	unsigned char	res17[0xec];
	unsigned int	epll_con0;
	unsigned int	epll_con1;
	unsigned int	epll_con2;
	unsigned char	res18[0x4];
	unsigned int	vpll_con0;
	unsigned int	vpll_con1;
	unsigned int	vpll_con2;
	unsigned char	res19[0xe4];
	unsigned int	src_top0;
	unsigned int	src_top1;
	unsigned char	res20[0x8];
	unsigned int	src_cam;
	unsigned int	src_tv;
	unsigned int	src_mfc;
	unsigned int	src_g3d;
	unsigned char	res21[0x4];
	unsigned int	src_lcd;
	unsigned int	src_isp;
	unsigned int	src_maudio;
	unsigned int	src_fsys;
	unsigned char	res22[0xc];
	unsigned int	src_peril0;
	unsigned int	src_peril1;
	unsigned int	src_cam1;
	unsigned char	res23[0xb4];
	unsigned int	src_mask_top;
	unsigned char	res24[0xc];
	unsigned int	src_mask_cam;
	unsigned int	src_mask_tv;
	unsigned char	res25[0xc];
	unsigned int	src_mask_lcd;
	unsigned int	src_mask_isp;
	unsigned int	src_mask_maudio;
	unsigned int	src_mask_fsys;
	unsigned char	res26[0xc];
	unsigned int	src_mask_peril0;
	unsigned int	src_mask_peril1;
	unsigned char	res27[0xb8];
	unsigned int	mux_stat_top0;
	unsigned int	mux_stat_top1;
	unsigned char	res28[0x10];
	unsigned int	mux_stat_mfc;
	unsigned int	mux_stat_g3d;
	unsigned char	res29[0x28];
	unsigned int	mux_stat_cam1;
	unsigned char	res30[0xb4];
	unsigned int	div_top;
	unsigned char	res31[0xc];
	unsigned int	div_cam;
	unsigned int	div_tv;
	unsigned int	div_mfc;
	unsigned int	div_g3d;
	unsigned char	res32[0x4];
	unsigned int	div_lcd;
	unsigned int	div_isp;
	unsigned int	div_maudio;
	unsigned int	div_fsys0;
	unsigned int	div_fsys1;
	unsigned int	div_fsys2;
	unsigned int	div_fsys3;
	unsigned int	div_peril0;
	unsigned int	div_peril1;
	unsigned int	div_peril2;
	unsigned int	div_peril3;
	unsigned int	div_peril4;
	unsigned int	div_peril5;
	unsigned int	div_cam1;
	unsigned char	res33[0x14];
	unsigned int	div2_ratio;
	unsigned char	res34[0x8c];
	unsigned int	div_stat_top;
	unsigned char	res35[0xc];
	unsigned int	div_stat_cam;
	unsigned int	div_stat_tv;
	unsigned int	div_stat_mfc;
	unsigned int	div_stat_g3d;
	unsigned char	res36[0x4];
	unsigned int	div_stat_lcd;
	unsigned int	div_stat_isp;
	unsigned int	div_stat_maudio;
	unsigned int	div_stat_fsys0;
	unsigned int	div_stat_fsys1;
	unsigned int	div_stat_fsys2;
	unsigned int	div_stat_fsys3;
	unsigned int	div_stat_peril0;
	unsigned int	div_stat_peril1;
	unsigned int	div_stat_peril2;
	unsigned int	div_stat_peril3;
	unsigned int	div_stat_peril4;
	unsigned int	div_stat_peril5;
	unsigned int	div_stat_cam1;
	unsigned char	res37[0x14];
	unsigned int	div2_stat;
	unsigned char	res38[0x29c];
	unsigned int	gate_ip_cam;
	unsigned int	gate_ip_tv;
	unsigned int	gate_ip_mfc;
	unsigned int	gate_ip_g3d;
	unsigned char	res39[0x4];
	unsigned int	gate_ip_lcd;
	unsigned int	gate_ip_isp;
	unsigned char	res40[0x4];
	unsigned int	gate_ip_fsys;
	unsigned char	res41[0x8];
	unsigned int	gate_ip_gps;
	unsigned int	gate_ip_peril;
	unsigned char	res42[0xc];
	unsigned char	res43[0x4];
	unsigned char	res44[0xc];
	unsigned int	gate_block;
	unsigned char	res45[0x8c];
	unsigned int	clkout_cmu_top;
	unsigned int	clkout_cmu_top_div_stat;
	unsigned char	res46[0x3600];
	unsigned int	mpll_lock;
	unsigned char	res47[0xfc];
	unsigned int	mpll_con0;
	unsigned int	mpll_con1;
	unsigned char	res48[0xf0];
	unsigned int	src_dmc;
	unsigned char	res49[0xfc];
	unsigned int	src_mask_dmc;
	unsigned char	res50[0xfc];
	unsigned int	mux_stat_dmc;
	unsigned char	res51[0xfc];
	unsigned int	div_dmc0;
	unsigned int	div_dmc1;
	unsigned char	res52[0xf8];
	unsigned int	div_stat_dmc0;
	unsigned int	div_stat_dmc1;
	unsigned char	res53[0xf8];
	unsigned int	gate_bus_dmc0;
	unsigned int	gate_bus_dmc1;
	unsigned char	res54[0x1f8];
	unsigned int	gate_ip_dmc0;
	unsigned int	gate_ip_dmc1;
	unsigned char	res55[0xf8];
	unsigned int	clkout_cmu_dmc;
	unsigned int	clkout_cmu_dmc_div_stat;
	unsigned char	res56[0x5f8];
	unsigned int	dcgidx_map0;
	unsigned int	dcgidx_map1;
	unsigned int	dcgidx_map2;
	unsigned char	res57[0x14];
	unsigned int	dcgperf_map0;
	unsigned int	dcgperf_map1;
	unsigned char	res58[0x18];
	unsigned int	dvcidx_map;
	unsigned char	res59[0x1c];
	unsigned int	freq_cpu;
	unsigned int	freq_dpm;
	unsigned char	res60[0x18];
	unsigned int	dvsemclk_en;
	unsigned int	maxperf;
	unsigned char	res61[0x8];
	unsigned int	dmc_freq_ctrl;
	unsigned int	dmc_pause_ctrl;
	unsigned int	dddrphy_lock_ctrl;
	unsigned int	c2c_state;
	unsigned char	res62[0x2f60];
	unsigned int	apll_lock;
	unsigned char	res63[0x8];
	unsigned char	res64[0xf4];
	unsigned int	apll_con0;
	unsigned int	apll_con1;
	unsigned char	res65[0xf8];
	unsigned int	src_cpu;
	unsigned char	res66[0x1fc];
	unsigned int	mux_stat_cpu;
	unsigned char	res67[0xfc];
	unsigned int	div_cpu0;
	unsigned int	div_cpu1;
	unsigned char	res68[0xf8];
	unsigned int	div_stat_cpu0;
	unsigned int	div_stat_cpu1;
	unsigned char	res69[0x2f8];
	unsigned int	clk_gate_ip_cpu;
	unsigned char	res70[0xfc];
	unsigned int	clkout_cmu_cpu;
	unsigned int	clkout_cmu_cpu_div_stat;
	unsigned char	res71[0x5f8];
	unsigned int	armclk_stopctrl;
	unsigned int	atclk_stopctrl;
	unsigned char	res72[0x10];
	unsigned char	res73[0x8];
	unsigned int	pwr_ctrl;
	unsigned int	pwr_ctrl2;
	unsigned char	res74[0xd8];
	unsigned int	apll_con0_l8;
	unsigned int	apll_con0_l7;
	unsigned int	apll_con0_l6;
	unsigned int	apll_con0_l5;
	unsigned int	apll_con0_l4;
	unsigned int	apll_con0_l3;
	unsigned int	apll_con0_l2;
	unsigned int	apll_con0_l1;
	unsigned int	iem_control;
	unsigned char	res75[0xdc];
	unsigned int	apll_con1_l8;
	unsigned int	apll_con1_l7;
	unsigned int	apll_con1_l6;
	unsigned int	apll_con1_l5;
	unsigned int	apll_con1_l4;
	unsigned int	apll_con1_l3;
	unsigned int	apll_con1_l2;
	unsigned int	apll_con1_l1;
	unsigned char	res76[0xe0];
	unsigned int	div_iem_l8;
	unsigned int	div_iem_l7;
	unsigned int	div_iem_l6;
	unsigned int	div_iem_l5;
	unsigned int	div_iem_l4;
	unsigned int	div_iem_l3;
	unsigned int	div_iem_l2;
	unsigned int	div_iem_l1;
	unsigned char	res77[0xe0];
	unsigned int	l2_status;
	unsigned char	res78[0xc];
	unsigned int	cpu_status;
	unsigned char	res79[0xc];
	unsigned int	ptm_status;
	unsigned char	res80[0x2edc];
	unsigned int	div_isp0;
	unsigned int	div_isp1;
	unsigned char	res81[0xf8];
	unsigned int	div_stat_isp0;
	unsigned int	div_stat_isp1;
	unsigned char	res82[0x3f8];
	unsigned int	gate_ip_isp0;
	unsigned int	gate_ip_isp1;
	unsigned char	res83[0x1f8];
	unsigned int	clkout_cmu_isp;
	unsigned int	clkout_cmu_ispd_div_stat;
	unsigned char	res84[0xf8];
	unsigned int	cmu_isp_spar0;
	unsigned int	cmu_isp_spar1;
	unsigned int	cmu_isp_spar2;
	unsigned int	cmu_isp_spar3;
};

struct s5pc100_clock {
	unsigned int	apll_lock;
	unsigned int	mpll_lock;
	unsigned int	epll_lock;
	unsigned int	hpll_lock;
	unsigned char	res1[0xf0];
	unsigned int	apll_con;
	unsigned int	mpll_con;
	unsigned int	epll_con;
	unsigned int	hpll_con;
	unsigned char	res2[0xf0];
	unsigned int	src0;
	unsigned int	src1;
	unsigned int	src2;
	unsigned int	src3;
	unsigned char	res3[0xf0];
	unsigned int	div0;
	unsigned int	div1;
	unsigned int	div2;
	unsigned int	div3;
	unsigned int	div4;
	unsigned char	res4[0x1ec];
	unsigned int	gate_d00;
	unsigned int	gate_d01;
	unsigned int	gate_d02;
	unsigned char	res5[0x54];
	unsigned int	gate_sclk0;
	unsigned int	gate_sclk1;
};

struct s5pc110_clock {
	unsigned int	apll_lock;
	unsigned char	res1[0x4];
	unsigned int	mpll_lock;
	unsigned char	res2[0x4];
	unsigned int	epll_lock;
	unsigned char	res3[0xc];
	unsigned int	vpll_lock;
	unsigned char	res4[0xdc];
	unsigned int	apll_con;
	unsigned char	res5[0x4];
	unsigned int	mpll_con;
	unsigned char	res6[0x4];
	unsigned int	epll_con;
	unsigned char	res7[0xc];
	unsigned int	vpll_con;
	unsigned char	res8[0xdc];
	unsigned int	src0;
	unsigned int	src1;
	unsigned int	src2;
	unsigned int	src3;
	unsigned char	res9[0xf0];
	unsigned int	div0;
	unsigned int	div1;
	unsigned int	div2;
	unsigned int	div3;
	unsigned int	div4;
	unsigned char	res10[0x1ec];
	unsigned int	gate_d00;
	unsigned int	gate_d01;
	unsigned int	gate_d02;
	unsigned char	res11[0x54];
	unsigned int	gate_sclk0;
	unsigned int	gate_sclk1;
};
#endif

#endif

/*
 * Copyright (C) 2012-2013 Freescale Semiconductor, Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
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
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/mx6.h>
#include <asm/arch/mx6_pins.h>
#if defined(CONFIG_SECURE_BOOT)
#include <asm/arch/mx6_secure.h>
#endif
#include <asm/arch/mx6dl_pins.h>
#include <asm/arch/iomux-v3.h>
#include <asm/arch/regs-anadig.h>
#include <asm/errno.h>
#ifdef CONFIG_MXC_FEC
#include <miiphy.h>
#endif

#if defined(CONFIG_VIDEO_MX5)
#include <asm/imx_pwm.h>
#include <linux/list.h>
#include <linux/fb.h>
#include <linux/mxcfb.h>
#include <ipu.h>
#endif
#if defined(CONFIG_VIDEO_MX5) || defined(CONFIG_MXC_EPDC)
#include <lcd.h>
#endif

#include "../../../drivers/video/mxc_epdc_fb.h"
#include <environment.h>

#ifdef CONFIG_IMX_ECSPI
#include <imx_spi.h>
#endif

#if CONFIG_I2C_MXC
#include <i2c.h>
#endif

#ifdef CONFIG_CMD_MMC
#include <mmc.h>
#include <fsl_esdhc.h>
#endif

#ifdef CONFIG_ARCH_MMU
#include <asm/mmu.h>
#include <asm/arch/mmu.h>
#endif

#ifdef CONFIG_CMD_CLOCK
#include <asm/clock.h>
#endif

#ifdef CONFIG_CMD_IMXOTP
#include <imx_otp.h>
#endif

#ifdef CONFIG_MXC_GPIO
#include <asm/gpio.h>
#include <asm/arch/gpio.h>
#endif

#ifdef CONFIG_ANDROID_RECOVERY
#include <recovery.h>
#endif

#ifdef CONFIG_DWC_AHSATA
#include <ahci.h>
#include <sata.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

static enum boot_device boot_dev;

#define GPIO_VOL_DN_KEY IMX_GPIO_NR(1, 5)
#define USB_OTG_PWR IMX_GPIO_NR(3, 22)
#define USB_H1_POWER IMX_GPIO_NR(1, 29)

extern int sata_curr_device;

#ifdef CONFIG_VIDEO_MX5
extern unsigned char fsl_bmp_reversed_600x400[];
extern int fsl_bmp_reversed_600x400_size;
extern int g_ipu_hw_rev;

#if defined(CONFIG_BMP_8BPP)
unsigned short colormap[256];
#elif defined(CONFIG_BMP_16BPP)
unsigned short colormap[65536];
#else
unsigned short colormap[16777216];
#endif

static struct pwm_device pwm0 = {
	.pwm_id = 0,
	.pwmo_invert = 0,
};

static int di = 1;

extern int ipuv3_fb_init(struct fb_videomode *mode, int di,
			int interface_pix_fmt,
			ipu_di_clk_parent_t di_clk_parent,
			int di_clk_val);

static struct fb_videomode lvds_xga = {
	 "XGA", 60, 1024, 768, 15385, 220, 40, 21, 7, 60, 10,
	 FB_SYNC_EXT,
	 FB_VMODE_NONINTERLACED,
	 0,
};

vidinfo_t panel_info;
#endif

static inline void setup_boot_device(void)
{
	uint soc_sbmr = readl(SRC_BASE_ADDR + 0x4);
	uint bt_mem_ctl = (soc_sbmr & 0x000000F0) >> 4 ;
	uint bt_mem_type = (soc_sbmr & 0x00000008) >> 3;
   //paolo -s
	uint bt_usdhc_num = (soc_sbmr & 0x00001800) >> 11; //paolo - boot_cfg2[4:3] -> usdhc port 0=usdhc1; 1=usdhc2; 2=usdhc3; 3==usdhc4;
	uint bt_port_select = (soc_sbmr & 0x05000000) >> 24; //paolo boot_cfg4[2:0] ->
   //paolo -e

	uint soc_sbmr2 = readl(SRC_BASE_ADDR + 0x1c);
	uint bmod = (soc_sbmr2 & 0x03000000) >> 24;

	if (bmod == 0x01)
	{
		boot_dev = 	USBOTG_BOOT;
		return;
	}

	switch (bt_mem_ctl) {
	case 0x0:
		if (bt_mem_type)
			boot_dev = ONE_NAND_BOOT;
		else
			boot_dev = WEIM_NOR_BOOT;
		break;
	case 0x2:
			boot_dev = SATA_BOOT;
		break;
	case 0x3:
	//paolo -s
		if (bt_port_select == 0) //paolo boot from ECSPI1 
			boot_dev = SPI_NOR_BOOT;
		else
			boot_dev = I2C_BOOT;
		break;
	//paolo -e
	case 0x4:
	case 0x5:
	//paolo -s
		if(bt_usdhc_num == 0)
			boot_dev = EXT_SD_BOOT;
		if(bt_usdhc_num == 3)
#ifdef QSEVEN 
		boot_dev = uSD_BOOT;
#endif
		
#ifdef UQSEVEN 
		boot_dev = EXT_SD_BOOT;
#endif

#ifdef uSBC 
                boot_dev = eMMC_BOOT;
#endif 
		break;
	case 0x6:
	case 0x7:
		//if(bt_usdhc_num == 2)
			boot_dev = eMMC_BOOT;
		break;
	case 0x8 ... 0xf:
		boot_dev = NAND_BOOT;
		break;
	default:
		boot_dev = UNKNOWN_BOOT;
		break;
	}
}

enum boot_device get_boot_device(void)
{
	return boot_dev;
}

u32 get_board_rev(void)
{
	return fsl_system_rev;
}

#ifdef CONFIG_ARCH_MMU
void board_mmu_init(void)
{
	unsigned long ttb_base = PHYS_SDRAM_1 + 0x4000;
	unsigned long i;

	/*
	* Set the TTB register
	*/
	asm volatile ("mcr  p15,0,%0,c2,c0,0" : : "r"(ttb_base) /*:*/);

	/*
	* Set the Domain Access Control Register
	*/
	i = ARM_ACCESS_DACR_DEFAULT;
	asm volatile ("mcr  p15,0,%0,c3,c0,0" : : "r"(i) /*:*/);

	/*
	* First clear all TT entries - ie Set them to Faulting
	*/
	memset((void *)ttb_base, 0, ARM_FIRST_LEVEL_PAGE_TABLE_SIZE);
	/* Actual   Virtual  Size   Attributes          Function */
	/* Base     Base     MB     cached? buffered?  access permissions */
	/* xxx00000 xxx00000 */
	X_ARM_MMU_SECTION(0x000, 0x000, 0x001,
			ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
			ARM_ACCESS_PERM_RW_RW); /* ROM, 1M */
	X_ARM_MMU_SECTION(0x001, 0x001, 0x008,
			ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
			ARM_ACCESS_PERM_RW_RW); /* 8M */
	X_ARM_MMU_SECTION(0x009, 0x009, 0x001,
			ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
			ARM_ACCESS_PERM_RW_RW); /* IRAM */
	X_ARM_MMU_SECTION(0x00A, 0x00A, 0x0F6,
			ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
			ARM_ACCESS_PERM_RW_RW); /* 246M */

	/* 2 GB memory starting at 0x10000000, only map 1.875 GB */
	X_ARM_MMU_SECTION(0x100, 0x100, 0x780,
			ARM_CACHEABLE, ARM_BUFFERABLE,
			ARM_ACCESS_PERM_RW_RW);
	/* uncached alias of the same 1.875 GB memory */
	X_ARM_MMU_SECTION(0x100, 0x880, 0x780,
			ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
			ARM_ACCESS_PERM_RW_RW);

	/* Enable MMU */
	MMU_ON();
}
#endif

#define ANATOP_PLL_LOCK                 0x80000000
#define ANATOP_PLL_ENABLE_MASK          0x00002000
#define ANATOP_PLL_BYPASS_MASK          0x00010000
#define ANATOP_PLL_LOCK                 0x80000000
#define ANATOP_PLL_PWDN_MASK            0x00001000
#define ANATOP_PLL_HOLD_RING_OFF_MASK   0x00000800
#define ANATOP_SATA_CLK_ENABLE_MASK     0x00100000

#ifdef CONFIG_DWC_AHSATA
/* Staggered Spin-up */
#define	HOST_CAP_SSS			(1 << 27)
/* host version register*/
#define	HOST_VERSIONR			0xfc
#define PORT_SATA_SR			0x128
#define	PORT_PHY_CTL			0x178
#define	PORT_PHY_CTL_PDDQ_LOC		0x100000

int sata_initialize(void)
{
	u32 reg = 0;
	u32 iterations = 1000000;

	if (sata_curr_device == -1) {
		/* Make sure that the PDDQ mode is disabled. */
		reg = readl(SATA_ARB_BASE_ADDR + PORT_PHY_CTL);
		writel(reg & (~PORT_PHY_CTL_PDDQ_LOC),
				SATA_ARB_BASE_ADDR + PORT_PHY_CTL);

		/* Reset HBA */
		writel(HOST_RESET, SATA_ARB_BASE_ADDR + HOST_CTL);

		reg = 0;
		while (readl(SATA_ARB_BASE_ADDR + HOST_VERSIONR) == 0) {
			reg++;
			if (reg > 1000000)
				break;
		}

		reg = readl(SATA_ARB_BASE_ADDR + HOST_CAP);
		if (!(reg & HOST_CAP_SSS)) {
			reg |= HOST_CAP_SSS;
			writel(reg, SATA_ARB_BASE_ADDR + HOST_CAP);
		}

		reg = readl(SATA_ARB_BASE_ADDR + HOST_PORTS_IMPL);
		if (!(reg & 0x1))
			writel((reg | 0x1),
					SATA_ARB_BASE_ADDR + HOST_PORTS_IMPL);

		/* Release resources when there is no device on the port */
		do {
			reg = readl(SATA_ARB_BASE_ADDR + PORT_SATA_SR) & 0xF;
			if ((reg & 0xF) == 0)
				iterations--;
			else
				break;

		} while (iterations > 0);
	}

	return __sata_initialize();
}

int setup_sata(void)
{
	u32 reg = 0;
	s32 timeout = 100000;

	/* Enable sata clock */
	reg = readl(CCM_BASE_ADDR + 0x7c); /* CCGR5 */
	reg |= 0x30;
	writel(reg, CCM_BASE_ADDR + 0x7c);

	/* Enable PLLs */
	reg = readl(ANATOP_BASE_ADDR + 0xe0); /* ENET PLL */
	reg &= ~ANATOP_PLL_PWDN_MASK;
	writel(reg, ANATOP_BASE_ADDR + 0xe0);
	reg |= ANATOP_PLL_ENABLE_MASK;
	while (timeout--) {
		if (readl(ANATOP_BASE_ADDR + 0xe0) & ANATOP_PLL_LOCK)
			break;
	}
	if (timeout <= 0)
		return -1;
	reg &= ~ANATOP_PLL_BYPASS_MASK;
	writel(reg, ANATOP_BASE_ADDR + 0xe0);
	reg |= ANATOP_SATA_CLK_ENABLE_MASK;
	writel(reg, ANATOP_BASE_ADDR + 0xe0);

	/* Enable sata phy */
	reg = readl(IOMUXC_BASE_ADDR + 0x34); /* GPR13 */

	reg &= ~0x07ffffff;
	/*
	 * rx_eq_val_0 = 5 [26:24]
	 * los_lvl = 0x12 [23:19]
	 * rx_dpll_mode_0 = 0x3 [18:16]
	 * mpll_ss_en = 0x0 [14]
	 * tx_atten_0 = 0x4 [13:11]
	 * tx_boost_0 = 0x0 [10:7]
	 * tx_lvl = 0x11 [6:2]
	 * mpll_ck_off_b = 0x1 [1]
	 * tx_edgerate_0 = 0x0 [0]
	 * */
	reg |= 0x0593A046;
	writel(reg, IOMUXC_BASE_ADDR + 0x34);

	if (sata_curr_device == -1) {
		/* Enable PDDQ mode in default if there isn't sata boot */
		reg = readl(SATA_ARB_BASE_ADDR + PORT_PHY_CTL);
		writel(reg | PORT_PHY_CTL_PDDQ_LOC,
				SATA_ARB_BASE_ADDR + PORT_PHY_CTL);
	}

	return 0;
}
#endif

int dram_init(void)
{
	/*
	 * Switch PL301_FAST2 to DDR Dual-channel mapping
	 * however this block the boot up, temperory redraw
	 */
	/*
	 * u32 reg = 1;
	 * writel(reg, GPV0_BASE_ADDR);
	 */

	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;

	return 0;
}

static void setup_uart(void)
{
#if defined CONFIG_MX6Q
	/* UART2 TXD */
	mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D26__UART2_TXD);

	/* UART2 RXD */
	mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D27__UART2_RXD);

	/* UART5 TXD */
	mxc_iomux_v3_setup_pad(MX6Q_PAD_KEY_COL1__UART5_TXD);

	/* UART5 RXD */
	mxc_iomux_v3_setup_pad(MX6Q_PAD_KEY_ROW1__UART5_RXD);

	/* UART5 RTS */
	//mxc_iomux_v3_setup_pad(MX6Q_PAD_KEY_COL4__UART5_RTS);

	/* UART5 CTS */
	//mxc_iomux_v3_setup_pad(MX6Q_PAD_KEY_ROW4__UART5_CTS);

#elif defined CONFIG_MX6DL
	/* UART2 TXD */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D26__UART2_TXD);

	/* UART2 RXD */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D27__UART2_RXD);

        /* UART5 TXD */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_KEY_COL1__UART5_TXD);

	/* UART5 RXD */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_KEY_ROW1__UART5_RXD);

	/* UART5 RTS */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_KEY_COL4__UART5_RTS);

	/* UART5 CTS */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_KEY_ROW4__UART5_CTS);
#endif
}

#ifdef CONFIG_VIDEO_MX5
void setup_lvds_poweron(void)
{
	int reg;
	/* AUX_5V_EN: GPIO(6, 10) */
#ifdef CONFIG_MX6DL
	mxc_iomux_v3_setup_pad(MX6DL_PAD_NANDF_RB0__GPIO_6_10);
#else
	mxc_iomux_v3_setup_pad(MX6Q_PAD_NANDF_RB0__GPIO_6_10);
#endif

	reg = readl(GPIO6_BASE_ADDR + GPIO_GDIR);
	reg |= (1 << 10);
	writel(reg, GPIO6_BASE_ADDR + GPIO_GDIR);

	reg = readl(GPIO6_BASE_ADDR + GPIO_DR);
	reg |= (1 << 10);
	writel(reg, GPIO6_BASE_ADDR + GPIO_DR);
}
#endif

#ifdef CONFIG_I2C_MXC

#ifdef QSEVEN

#define I2C1_SDA_GPIO_BIT_MASK  (1 << 28)
#define I2C1_SCL_GPIO_BIT_MASK  (1 << 21)
#define I2C2_SCL_GPIO_BIT_MASK  (1 << 12)
#define I2C2_SDA_GPIO_BIT_MASK  (1 << 13)
#define I2C3_SCL_GPIO_BIT_MASK   (1 << 6)
#define I2C3_SDA_GPIO_BIT_MASK  (1 << 11)


static void setup_i2c(unsigned int module_base)
{
	unsigned int reg;

	switch (module_base) {
	case I2C1_BASE_ADDR:
#if defined CONFIG_MX6Q
		/* i2c1 SDA */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D28__I2C1_SDA);

		/* i2c1 SCL */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D21__I2C1_SCL);
#elif defined CONFIG_MX6DL
		/* i2c1 SDA */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D28__I2C1_SDA);
		/* i2c1 SCL */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D21__I2C1_SCL);
#endif

		/* Enable i2c clock */
		reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR2);
		reg |= 0xC0;
		writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR2);

		break;
	case I2C2_BASE_ADDR:
#if defined CONFIG_MX6Q
		/* i2c2 SDA */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_KEY_ROW3__I2C2_SDA);

		/* i2c2 SCL */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_KEY_COL3__I2C2_SCL);
#elif defined CONFIG_MX6DL
		/* i2c2 SDA */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_KEY_ROW3__I2C2_SDA);

		/* i2c2 SCL */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_KEY_COL3__I2C2_SCL);
#endif

		/* Enable i2c clock */
		reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR2);
		reg |= 0x300;
		writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR2);

		break;
	case I2C3_BASE_ADDR:
#if defined CONFIG_MX6Q
		/* GPIO_3 for I2C3_SCL */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_GPIO_5__I2C3_SCL);
		/* GPIO_6 for I2C3_SDA */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_GPIO_16__I2C3_SDA);

#elif defined CONFIG_MX6DL
		/* GPIO_3 for I2C3_SCL */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_GPIO_5__I2C3_SCL);
		/* GPIO_6 for I2C3_SDA */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_GPIO_16__I2C3_SDA);
#endif
		/* Enable i2c clock */
		reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR2);
		reg |= 0xC00;
		writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR2);

		break;
	default:
		printf("Invalid I2C base: 0x%x\n", module_base);
		break;
	}
}
/* Note: udelay() is not accurate for i2c timing */
static void __udelay(int time)
{
	int i, j;

	for (i = 0; i < time; i++) {
		for (j = 0; j < 200; j++) {
			asm("nop");
			asm("nop");
		}
	}
}
static void mx6q_i2c_gpio_scl_direction(int bus, int output)
{
	u32 reg;

	switch (bus) {
	case 1:
#if defined CONFIG_MX6Q
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D21__GPIO_3_21);
#elif defined CONFIG_MX6DL
		mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D21__GPIO_3_21);
#endif
		reg = readl(GPIO3_BASE_ADDR + GPIO_GDIR);
		if (output)
			reg |= I2C1_SCL_GPIO_BIT_MASK;
		else
			reg &= ~I2C1_SCL_GPIO_BIT_MASK;
		writel(reg, GPIO3_BASE_ADDR + GPIO_GDIR);
		break;
	case 2:
#if defined CONFIG_MX6Q
		mxc_iomux_v3_setup_pad(MX6Q_PAD_KEY_COL3__GPIO_4_12);
#elif defined CONFIG_MX6DL
		mxc_iomux_v3_setup_pad(MX6DL_PAD_KEY_COL3__GPIO_4_12);
#endif
		reg = readl(GPIO4_BASE_ADDR + GPIO_GDIR);
		if (output)
			reg |= I2C2_SCL_GPIO_BIT_MASK;
		else
			reg &= ~I2C2_SCL_GPIO_BIT_MASK;
		writel(reg, GPIO4_BASE_ADDR + GPIO_GDIR);
		break;
	case 3:
#if defined CONFIG_MX6Q
		mxc_iomux_v3_setup_pad(MX6Q_PAD_GPIO_5__GPIO_1_5);
#elif defined CONFIG_MX6DL
		mxc_iomux_v3_setup_pad(MX6DL_PAD_GPIO_5__GPIO_1_5);
#endif
		reg = readl(GPIO1_BASE_ADDR + GPIO_GDIR);
		if (output)
			reg |= I2C3_SCL_GPIO_BIT_MASK;
		else
			reg &= I2C3_SCL_GPIO_BIT_MASK;
		writel(reg, GPIO1_BASE_ADDR + GPIO_GDIR);
		break;
	}
}

/* set 1 to output, sent 0 to input */
static void mx6q_i2c_gpio_sda_direction(int bus, int output)
{
	u32 reg;

	switch (bus) {
	case 1:
#if defined CONFIG_MX6Q
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D28__GPIO_3_28);
#elif defined CONFIG_MX6DL
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D28__GPIO_3_28);
#endif
		reg = readl(GPIO3_BASE_ADDR + GPIO_GDIR);
		if (output)
			reg |= I2C1_SDA_GPIO_BIT_MASK;
		else
			reg &= ~I2C1_SDA_GPIO_BIT_MASK;
		writel(reg, GPIO3_BASE_ADDR + GPIO_GDIR);
		break;
	case 2:
#if defined CONFIG_MX6Q
		mxc_iomux_v3_setup_pad(MX6Q_PAD_KEY_ROW3__GPIO_4_13);
#elif defined CONFIG_MX6DL
		mxc_iomux_v3_setup_pad(MX6DL_PAD_KEY_ROW3__GPIO_4_13);
#endif
		reg = readl(GPIO4_BASE_ADDR + GPIO_GDIR);
		if (output)
			reg |= I2C2_SDA_GPIO_BIT_MASK;
		else
			reg &= ~I2C2_SDA_GPIO_BIT_MASK;
		writel(reg, GPIO4_BASE_ADDR + GPIO_GDIR);
	case 3:
#if defined CONFIG_MX6Q
		mxc_iomux_v3_setup_pad(MX6Q_PAD_GPIO_16__GPIO_7_11);
#elif defined CONFIG_MX6DL
		mxc_iomux_v3_setup_pad(MX6DL_PAD_GPIO_16__GPIO_7_11);
#endif
		reg = readl(GPIO7_BASE_ADDR + GPIO_GDIR);
		if (output)
			reg |= I2C3_SDA_GPIO_BIT_MASK;
		else
			reg &= ~I2C3_SDA_GPIO_BIT_MASK;
		writel(reg, GPIO7_BASE_ADDR + GPIO_GDIR);
	default:
		break;
	}
}

/* set 1 to high 0 to low */
static void mx6q_i2c_gpio_scl_set_level(int bus, int high)
{
	u32 reg;

	switch (bus) {
	case 1:
		reg = readl(GPIO3_BASE_ADDR + GPIO_DR);
		if (high)
			reg |= I2C1_SCL_GPIO_BIT_MASK;
		else
			reg &= ~I2C1_SCL_GPIO_BIT_MASK;
		writel(reg, GPIO3_BASE_ADDR + GPIO_DR);
		break;
	case 2:
		reg = readl(GPIO4_BASE_ADDR + GPIO_DR);
		if (high)
			reg |= I2C2_SCL_GPIO_BIT_MASK;
		else
			reg &= ~I2C2_SCL_GPIO_BIT_MASK;
		writel(reg, GPIO4_BASE_ADDR + GPIO_DR);
		break;
	case 3:
		reg = readl(GPIO7_BASE_ADDR + GPIO_DR);
		if (high)
			reg |= I2C3_SCL_GPIO_BIT_MASK;
		else
			reg &= ~I2C3_SCL_GPIO_BIT_MASK;
		writel(reg, GPIO7_BASE_ADDR + GPIO_DR);
		break;
	}
}

/* set 1 to high 0 to low */
static void mx6q_i2c_gpio_sda_set_level(int bus, int high)
{
	u32 reg;

	switch (bus) {
	case 1:
		reg = readl(GPIO3_BASE_ADDR + GPIO_DR);
		if (high)
			reg |= I2C1_SDA_GPIO_BIT_MASK;
		else
			reg &= ~I2C1_SDA_GPIO_BIT_MASK;
		writel(reg, GPIO3_BASE_ADDR + GPIO_DR);
		break;
	case 2:
		reg = readl(GPIO4_BASE_ADDR + GPIO_DR);
		if (high)
			reg |= I2C2_SDA_GPIO_BIT_MASK;
		else
			reg &= ~I2C2_SDA_GPIO_BIT_MASK;
		writel(reg, GPIO4_BASE_ADDR + GPIO_DR);
		break;
	case 3:
		reg = readl(GPIO7_BASE_ADDR + GPIO_DR);
		if (high)
			reg |= I2C3_SDA_GPIO_BIT_MASK;
		else
			reg &= ~I2C3_SDA_GPIO_BIT_MASK;
		writel(reg, GPIO7_BASE_ADDR + GPIO_DR);
		break;
	}
}

static int mx6q_i2c_gpio_check_sda(int bus)
{
	u32 reg;
	int result = 0;

	switch (bus) {
	case 1:
		reg = readl(GPIO3_BASE_ADDR + GPIO_PSR);
		result = !!(reg & I2C1_SDA_GPIO_BIT_MASK);
		break;
	case 2:
		reg = readl(GPIO4_BASE_ADDR + GPIO_PSR);
		result = !!(reg & I2C2_SDA_GPIO_BIT_MASK);
		break;
	case 3:
		reg = readl(GPIO7_BASE_ADDR + GPIO_PSR);
		result = !!(reg & I2C3_SDA_GPIO_BIT_MASK);
		break;
	}

	return result;
}

#endif



#if defined (UQSEVEN) || defined (uSBC)

#define I2C1_SDA_GPIO_BIT_MASK  (1 << 28)
#define I2C1_SCL_GPIO_BIT_MASK  (1 << 21)
#define I2C2_SDA_GPIO_BIT_MASK  (1 << 13)
#define I2C2_SCL_GPIO_BIT_MASK  (1 << 30)
#define I2C3_SDA_GPIO_BIT_MASK  (1 << 11)
#define I2C3_SCL_GPIO_BIT_MASK  (1 << 4)


static void setup_i2c(unsigned int module_base)
{
	unsigned int reg;

	switch (module_base) {
	case I2C1_BASE_ADDR:
#if defined CONFIG_MX6Q
		/* i2c1 SDA */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D28__I2C1_SDA);
		/* i2c1 SCL */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D21__I2C1_SCL);
#elif defined CONFIG_MX6DL
		/* i2c1 SDA */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D28__I2C1_SDA);
		/* i2c1 SCL */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D21__I2C1_SCL);
#endif

		/* Enable i2c clock */
		reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR2);
		reg |= 0xC0;
		writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR2);

		break;
	case I2C2_BASE_ADDR:
#if defined CONFIG_MX6Q
		/* i2c2 SDA */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_KEY_ROW3__I2C2_SDA);
		/* i2c2 SCL */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_EB2__I2C2_SCL);
#elif defined CONFIG_MX6DL
		/* i2c2 SDA */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_KEY_ROW3__I2C2_SDA);
		/* i2c2 SCL */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_EB2__I2C2_SCL);
#endif

		/* Enable i2c clock */
		reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR2);
		reg |= 0x300;
		writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR2);

		break;
	case I2C3_BASE_ADDR:
#if defined CONFIG_MX6Q
		/* GPIO_3 for I2C3_SCL */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_GPIO_5__I2C3_SCL);
		/* GPIO_6 for I2C3_SDA */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_GPIO_16__I2C3_SDA);
#elif defined CONFIG_MX6DL
		/* GPIO_3 for I2C3_SCL */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_GPIO_5__I2C3_SCL);
		/* GPIO_6 for I2C3_SDA */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_GPIO_16__I2C3_SDA);
#endif
		/* Enable i2c clock */
		reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR2);
		reg |= 0xC00;
		writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR2);

		break;
	default:
		printf("Invalid I2C base: 0x%x\n", module_base);
		break;
	}
}
/* Note: udelay() is not accurate for i2c timing */
static void __udelay(int time)
{
	int i, j;

	for (i = 0; i < time; i++) {
		for (j = 0; j < 200; j++) {
			asm("nop");
			asm("nop");
		}
	}
}
static void mx6q_i2c_gpio_scl_direction(int bus, int output)
{
	u32 reg;

	switch (bus) {
	case 1:
#if defined CONFIG_MX6Q
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D21__GPIO_3_21);
#elif defined CONFIG_MX6DL
		mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D21__GPIO_3_21);
#endif
		reg = readl(GPIO3_BASE_ADDR + GPIO_GDIR);
		if (output)
			reg |= I2C1_SCL_GPIO_BIT_MASK;
		else
			reg &= ~I2C1_SCL_GPIO_BIT_MASK;
		writel(reg, GPIO3_BASE_ADDR + GPIO_GDIR);
		break;
	case 2:
#if defined CONFIG_MX6Q
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_EB2__GPIO_2_30);
#elif defined CONFIG_MX6DL
		mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_EB2__GPIO_2_30);
#endif
		reg = readl(GPIO2_BASE_ADDR + GPIO_GDIR);
		if (output)
			reg |= I2C2_SCL_GPIO_BIT_MASK;
		else
			reg &= ~I2C2_SCL_GPIO_BIT_MASK;
		writel(reg, GPIO2_BASE_ADDR + GPIO_GDIR);
		break;
	case 3:
#if defined CONFIG_MX6Q
		mxc_iomux_v3_setup_pad(MX6Q_PAD_GPIO_5__GPIO_1_5);
#elif defined CONFIG_MX6DL
		mxc_iomux_v3_setup_pad(MX6DL_PAD_GPIO_5__GPIO_1_5);
#endif
		reg = readl(GPIO5_BASE_ADDR + GPIO_GDIR);
		if (output)
			reg |= I2C3_SCL_GPIO_BIT_MASK;
		else
			reg &= I2C3_SCL_GPIO_BIT_MASK;
		writel(reg, GPIO5_BASE_ADDR + GPIO_GDIR);
		break;
	}
}

/* set 1 to output, sent 0 to input */
static void mx6q_i2c_gpio_sda_direction(int bus, int output)
{
	u32 reg;

	switch (bus) {
	case 1:
#if defined CONFIG_MX6Q
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D28__GPIO_3_28);
#elif defined CONFIG_MX6DL
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D28__GPIO_3_28);
#endif
		reg = readl(GPIO3_BASE_ADDR + GPIO_GDIR);
		if (output)
			reg |= I2C1_SDA_GPIO_BIT_MASK;
		else
			reg &= ~I2C1_SDA_GPIO_BIT_MASK;
		writel(reg, GPIO3_BASE_ADDR + GPIO_GDIR);
		break;
	case 2:
#if defined CONFIG_MX6Q
		mxc_iomux_v3_setup_pad(MX6Q_PAD_KEY_ROW3__GPIO_4_13);
#elif defined CONFIG_MX6DL
		mxc_iomux_v3_setup_pad(MX6DL_PAD_KEY_ROW3__GPIO_4_13);
#endif
		reg = readl(GPIO4_BASE_ADDR + GPIO_GDIR);
		if (output)
			reg |= I2C2_SDA_GPIO_BIT_MASK;
		else
			reg &= ~I2C2_SDA_GPIO_BIT_MASK;
		writel(reg, GPIO4_BASE_ADDR + GPIO_GDIR);
	case 3:
#if defined CONFIG_MX6Q
		mxc_iomux_v3_setup_pad(MX6Q_PAD_GPIO_16__GPIO_7_11);
#elif defined CONFIG_MX6DL
		mxc_iomux_v3_setup_pad(MX6DL_PAD_GPIO_16__GPIO_7_11);
#endif
		reg = readl(GPIO7_BASE_ADDR + GPIO_GDIR);
		if (output)
			reg |= I2C3_SDA_GPIO_BIT_MASK;
		else
			reg &= ~I2C3_SDA_GPIO_BIT_MASK;
		writel(reg, GPIO7_BASE_ADDR + GPIO_GDIR);
	default:
		break;
	}
}

/* set 1 to high 0 to low */
static void mx6q_i2c_gpio_scl_set_level(int bus, int high)
{
	u32 reg;

	switch (bus) {
	case 1:
		reg = readl(GPIO3_BASE_ADDR + GPIO_DR);
		if (high)
			reg |= I2C1_SCL_GPIO_BIT_MASK;
		else
			reg &= ~I2C1_SCL_GPIO_BIT_MASK;
		writel(reg, GPIO3_BASE_ADDR + GPIO_DR);
		break;
	case 2:
		reg = readl(GPIO2_BASE_ADDR + GPIO_DR);
		if (high)
			reg |= I2C2_SCL_GPIO_BIT_MASK;
		else
			reg &= ~I2C2_SCL_GPIO_BIT_MASK;
		writel(reg, GPIO2_BASE_ADDR + GPIO_DR);
		break;
	case 3:
		reg = readl(GPIO5_BASE_ADDR + GPIO_DR);
		if (high)
			reg |= I2C3_SCL_GPIO_BIT_MASK;
		else
			reg &= ~I2C3_SCL_GPIO_BIT_MASK;
		writel(reg, GPIO5_BASE_ADDR + GPIO_DR);
		break;
	}
}

/* set 1 to high 0 to low */
static void mx6q_i2c_gpio_sda_set_level(int bus, int high)
{
	u32 reg;

	switch (bus) {
	case 1:
		reg = readl(GPIO3_BASE_ADDR + GPIO_DR);
		if (high)
			reg |= I2C1_SDA_GPIO_BIT_MASK;
		else
			reg &= ~I2C1_SDA_GPIO_BIT_MASK;
		writel(reg, GPIO3_BASE_ADDR + GPIO_DR);
		break;
	case 2:
		reg = readl(GPIO4_BASE_ADDR + GPIO_DR);
		if (high)
			reg |= I2C2_SDA_GPIO_BIT_MASK;
		else
			reg &= ~I2C2_SDA_GPIO_BIT_MASK;
		writel(reg, GPIO4_BASE_ADDR + GPIO_DR);
		break;
	case 3:
		reg = readl(GPIO7_BASE_ADDR + GPIO_DR);
		if (high)
			reg |= I2C3_SDA_GPIO_BIT_MASK;
		else
			reg &= ~I2C3_SDA_GPIO_BIT_MASK;
		writel(reg, GPIO7_BASE_ADDR + GPIO_DR);
		break;
	}
}

static int mx6q_i2c_gpio_check_sda(int bus)
{
	u32 reg;
	int result = 0;

	switch (bus) {
	case 1:
		reg = readl(GPIO3_BASE_ADDR + GPIO_PSR);
		result = !!(reg & I2C1_SDA_GPIO_BIT_MASK);
		break;
	case 2:
		reg = readl(GPIO4_BASE_ADDR + GPIO_PSR);
		result = !!(reg & I2C2_SDA_GPIO_BIT_MASK);
		break;
	case 3:
		reg = readl(GPIO7_BASE_ADDR + GPIO_PSR);
		result = !!(reg & I2C3_SDA_GPIO_BIT_MASK);
		break;
	}

	return result;
}

#endif

 /* Random reboot cause i2c SDA low issue:
  * the i2c bus busy because some device pull down the I2C SDA
  * line. This happens when Host is reading some byte from slave, and
  * then host is reset/reboot. Since in this case, device is
  * controlling i2c SDA line, the only thing host can do this give the
  * clock on SCL and sending NAK, and STOP to finish this
  * transaction.
  *
  * How to fix this issue:
  * detect if the SDA was low on bus send 8 dummy clock, and 1
  * clock + NAK, and STOP to finish i2c transaction the pending
  * transfer.
  */
int i2c_bus_recovery(void)
{
	int i, bus, result = 0;

	for (bus = 1; bus <= 3; bus++) {
		mx6q_i2c_gpio_sda_direction(bus, 0);

		if (mx6q_i2c_gpio_check_sda(bus) == 0) {
			printf("i2c: I2C%d SDA is low, start i2c recovery...\n", bus);
			mx6q_i2c_gpio_scl_direction(bus, 1);
			mx6q_i2c_gpio_scl_set_level(bus, 1);
			__udelay(10000);

			for (i = 0; i < 9; i++) {
				mx6q_i2c_gpio_scl_set_level(bus, 1);
				__udelay(5);
				mx6q_i2c_gpio_scl_set_level(bus, 0);
				__udelay(5);
			}

			/* 9th clock here, the slave should already
			   release the SDA, we can set SDA as high to
			   a NAK.*/
			mx6q_i2c_gpio_sda_direction(bus, 1);
			mx6q_i2c_gpio_sda_set_level(bus, 1);
			__udelay(1); /* Pull up SDA first */
			mx6q_i2c_gpio_scl_set_level(bus, 1);
			__udelay(5); /* plus pervious 1 us */
			mx6q_i2c_gpio_scl_set_level(bus, 0);
			__udelay(5);
			mx6q_i2c_gpio_sda_set_level(bus, 0);
			__udelay(5);
			mx6q_i2c_gpio_scl_set_level(bus, 1);
			__udelay(5);
			/* Here: SCL is high, and SDA from low to high, it's a
			 * stop condition */
			mx6q_i2c_gpio_sda_set_level(bus, 1);
			__udelay(5);

			mx6q_i2c_gpio_sda_direction(bus, 0);
			if (mx6q_i2c_gpio_check_sda(bus) == 1)
				printf("I2C%d Recovery success\n", bus);
			else {
				printf("I2C%d Recovery failed, I2C1 SDA still low!!!\n", bus);
				result |= 1 << bus;
			}
		}

		/* configure back to i2c */
		switch (bus) {
		case 1:
			setup_i2c(I2C1_BASE_ADDR);
			break;
		case 2:
			setup_i2c(I2C2_BASE_ADDR);
			break;
		case 3:
			setup_i2c(I2C3_BASE_ADDR);
			break;
		}
	}

	return result;
}


static int setup_pmic_voltages(void)
{
	unsigned char value, rev_id = 0 ;
	i2c_init(CONFIG_SYS_I2C_SPEED, CONFIG_SYS_I2C_SLAVE);
	if (!i2c_probe(0x8)) {
		if (i2c_read(0x8, 0, 1, &value, 1)) {
			printf("Read device ID error!\n");
			return -1;
		}
		if (i2c_read(0x8, 3, 1, &rev_id, 1)) {
			printf("Read Rev ID error!\n");
			return -1;
		}
		printf("Found PFUZE100! deviceid=%x,revid=%x\n", value, rev_id);
		/*For camera streaks issue,swap VGEN5 and VGEN3 to power camera.
		*sperate VDDHIGH_IN and camera 2.8V power supply, after switch:
		*VGEN5 for VDDHIGH_IN and increase to 3V to align with datasheet
		*VGEN3 for camera 2.8V power supply
		*/
		/*increase VGEN3 from 2.5 to 2.8V*/
		if (i2c_read(0x8, 0x6e, 1, &value, 1)) {
			printf("Read VGEN3 error!\n");
			return -1;
		}
		value &= ~0xf;
		value |= 0xa;
		if (i2c_write(0x8, 0x6e, 1, &value, 1)) {
			printf("Set VGEN3 error!\n");
			return -1;
		}
		/*increase VGEN5 from 2.8 to 3V*/
		if (i2c_read(0x8, 0x70, 1, &value, 1)) {
			printf("Read VGEN5 error!\n");
			return -1;
		}
		value &= ~0xf;
		value |= 0xc;
		if (i2c_write(0x8, 0x70, 1, &value, 1)) {
			printf("Set VGEN5 error!\n");
			return -1;
		}
	}
	return 0;
}
#endif

#ifdef CONFIG_IMX_ECSPI
s32 spi_get_cfg(struct imx_spi_dev_t *dev)
{
	switch (dev->slave.cs) {
	case 0:
		/* SPI-NOR */
		dev->base = ECSPI1_BASE_ADDR;
		dev->freq = 25000000;
		dev->ss_pol = IMX_SPI_ACTIVE_LOW;
		dev->ss = 0;
		dev->fifo_sz = 64 * 4;
		dev->us_delay = 0;
		break;
	case 1:
		/* SPI-NOR */
		dev->base = ECSPI1_BASE_ADDR;
		dev->freq = 25000000;
		dev->ss_pol = IMX_SPI_ACTIVE_LOW;
		dev->ss = 1;
		dev->fifo_sz = 64 * 4;
		dev->us_delay = 0;
		break;
	case 3:
#if defined (CONFIG_MX6Q_SECO_Q7) || defined  (CONFIG_MX6DL_SECO_Q7) || defined  (CONFIG_MX6S_SECO_Q7)	  
		/* SPI-RTC */
                dev->base = ECSPI1_BASE_ADDR;
                dev->freq = 1000000;
                dev->ss_pol = IMX_SPI_ACTIVE_HIGH;
                dev->ss = 3;
                dev->fifo_sz = 64 * 4;
                dev->us_delay = 0;
                break;
#elif defined (CONFIG_MX6Q_SECO_UQ7) || defined  (CONFIG_MX6DL_SECO_UQ7) || defined  (CONFIG_MX6S_SECO_UQ7)
		/* SPI-RTC */
                dev->base = ECSPI3_BASE_ADDR;
                dev->freq = 1000000;
                dev->ss_pol = IMX_SPI_ACTIVE_HIGH;
                dev->ss = 3;
                dev->fifo_sz = 64 * 4;
                dev->us_delay = 0;
                break;
#elif defined (CONFIG_MX6Q_SECO_uSBC) || defined  (CONFIG_MX6DL_SECO_uSBC) || defined  (CONFIG_MX6S_SECO_uSBC)
                /* SPI-RTC */
                dev->base = ECSPI3_BASE_ADDR;
                dev->freq = 1000000;
                dev->ss_pol = IMX_SPI_ACTIVE_HIGH;
                dev->ss = 3;
                dev->fifo_sz = 64 * 4;
                dev->us_delay = 0;
                break;
#endif		
	default:
		printf("Invalid Bus ID!\n");
	}

	return 0;
}

void spi_io_init(struct imx_spi_dev_t *dev)
{
	u32 reg;

	switch (dev->base) {
	case ECSPI1_BASE_ADDR:
		/* Enable clock */
		reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR1);
		reg |= 0x3;
		writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR1);

#if defined CONFIG_MX6Q_SECO_Q7
		/* SCLK */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D16__ECSPI1_SCLK);

		/* MISO */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D17__ECSPI1_MISO);

		/* MOSI */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D18__ECSPI1_MOSI);

		if (dev->ss == 1){
			mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D19__ECSPI1_SS1);
		}
		if (dev->ss == 3){
                        mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_A21__GPIO_2_17);
		}
#elif defined (CONFIG_MX6DL_SECO_Q7) || defined (CONFIG_MX6S_SECO_Q7)
		/* SCLK */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D16__ECSPI1_SCLK);

		/* MISO */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D17__ECSPI1_MISO);

		/* MOSI */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D18__ECSPI1_MOSI);

		if (dev->ss == 1)
			mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D19__ECSPI1_SS1);
		if (dev->ss == 3)
                        mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_A21__GPIO_2_17);
#endif
		
		break;
	case ECSPI2_BASE_ADDR:
	case ECSPI3_BASE_ADDR:
                /* Enable SPI3 clock*/
                reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR1);
                reg |= 0x30;
                writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR1);

#if defined CONFIG_MX6Q_SECO_UQ7
                /* SCLK */
                mxc_iomux_v3_setup_pad(MX6Q_PAD_DISP0_DAT0__ECSPI3_SCLK);

                /* MISO */
                mxc_iomux_v3_setup_pad(MX6Q_PAD_DISP0_DAT2__ECSPI3_MISO);

                /* MOSI */
                mxc_iomux_v3_setup_pad(MX6Q_PAD_DISP0_DAT1__ECSPI3_MOSI);

                if (dev->ss == 3)
                        mxc_iomux_v3_setup_pad(MX6Q_PAD_DISP0_DAT3__GPIO_4_24);
               
#elif defined (CONFIG_MX6DL_SECO_UQ7) || defined (CONFIG_MX6S_SECO_UQ7)
                /* SCLK */
                mxc_iomux_v3_setup_pad(MX6DL_PAD_DISP0_DAT0__ECSPI3_SCLK);

                /* MISO */
                mxc_iomux_v3_setup_pad(MX6DL_PAD_DISP0_DAT2__ECSPI3_MISO);

                /* MOSI */
                mxc_iomux_v3_setup_pad(MX6DL_PAD_DISP0_DAT1__ECSPI3_MOSI);

                if (dev->ss == 3)
                        mxc_iomux_v3_setup_pad(MX6DL_PAD_DISP0_DAT3__GPIO_4_24);
#endif 
#if defined CONFIG_MX6Q_SECO_uSBC
                /* SCLK */
                mxc_iomux_v3_setup_pad(MX6Q_PAD_DISP0_DAT0__ECSPI3_SCLK);

                /* MISO */
                mxc_iomux_v3_setup_pad(MX6Q_PAD_DISP0_DAT2__ECSPI3_MISO);

                /* MOSI */
                mxc_iomux_v3_setup_pad(MX6Q_PAD_DISP0_DAT1__ECSPI3_MOSI);

                if (dev->ss == 3)
                        mxc_iomux_v3_setup_pad(MX6Q_PAD_DISP0_DAT3__GPIO_4_24);

#elif defined (CONFIG_MX6DL_SECO_uSBC) || defined (CONFIG_MX6S_SECO_uSBC)
                /* SCLK */
                mxc_iomux_v3_setup_pad(MX6DL_PAD_DISP0_DAT0__ECSPI3_SCLK);

                /* MISO */
                mxc_iomux_v3_setup_pad(MX6DL_PAD_DISP0_DAT2__ECSPI3_MISO);

                /* MOSI */
                mxc_iomux_v3_setup_pad(MX6DL_PAD_DISP0_DAT1__ECSPI3_MOSI);

                if (dev->ss == 3)
                        mxc_iomux_v3_setup_pad(MX6DL_PAD_DISP0_DAT3__GPIO_4_24);
#endif

		/* ecspi2-3 fall through */
		break;
	default:
		break;
	}
}
#endif
#if 0
#ifdef CONFIG_NAND_GPMI

iomux_v3_cfg_t nfc_pads[] = {
	MX6Q_PAD_NANDF_CLE__RAWNAND_CLE,
	MX6Q_PAD_NANDF_ALE__RAWNAND_ALE,
	MX6Q_PAD_NANDF_WP_B__RAWNAND_RESETN,
	MX6Q_PAD_NANDF_RB0__RAWNAND_READY0,
	MX6Q_PAD_NANDF_CS0__RAWNAND_CE0N,
	MX6Q_PAD_NANDF_CS1__RAWNAND_CE1N,
	MX6Q_PAD_NANDF_CS2__RAWNAND_CE2N,
	MX6Q_PAD_NANDF_CS3__RAWNAND_CE3N,
	MX6Q_PAD_SD4_CMD__RAWNAND_RDN,
	MX6Q_PAD_SD4_CLK__RAWNAND_WRN,
	MX6Q_PAD_NANDF_D0__RAWNAND_D0,
	MX6Q_PAD_NANDF_D1__RAWNAND_D1,
	MX6Q_PAD_NANDF_D2__RAWNAND_D2,
	MX6Q_PAD_NANDF_D3__RAWNAND_D3,
	MX6Q_PAD_NANDF_D4__RAWNAND_D4,
	MX6Q_PAD_NANDF_D5__RAWNAND_D5,
	MX6Q_PAD_NANDF_D6__RAWNAND_D6,
	MX6Q_PAD_NANDF_D7__RAWNAND_D7,
	MX6Q_PAD_SD4_DAT0__RAWNAND_DQS,
};

int setup_gpmi_nand(void)
{
	unsigned int reg;

	/* config gpmi nand iomux */
	mxc_iomux_v3_setup_multiple_pads(nfc_pads,
			ARRAY_SIZE(nfc_pads));


	/* config gpmi and bch clock to 11Mhz*/
	reg = readl(CCM_BASE_ADDR + CLKCTL_CS2CDR);
	reg &= 0xF800FFFF;
	reg |= 0x01E40000;
	writel(reg, CCM_BASE_ADDR + CLKCTL_CS2CDR);

	/* enable gpmi and bch clock gating */
	reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR4);
	reg |= 0xFF003000;
	writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR4);

	/* enable apbh clock gating */
	reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR0);
	reg |= 0x0030;
	writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR0);

}
#endif
#endif

#ifdef CONFIG_NET_MULTI
int board_eth_init(bd_t *bis)
{
	int rc = -ENODEV;

	return rc;
}
#endif

#ifdef CONFIG_CMD_MMC

/* On this board, only SD3 can support 1.8V signalling
 * that is required for UHS-I mode of operation.
 * Last element in struct is used to indicate 1.8V support.
 */
struct fsl_esdhc_cfg usdhc_cfg[4] = {
	{USDHC1_BASE_ADDR, 1, 1, 1, 0},
	{USDHC2_BASE_ADDR, 1, 1, 1, 0},
	{USDHC3_BASE_ADDR, 1, 1, 1, 0},
	{USDHC4_BASE_ADDR, 1, 1, 1, 0},
};

#if defined CONFIG_MX6Q
iomux_v3_cfg_t usdhc1_pads[] = {
	MX6Q_PAD_SD1_CLK__USDHC1_CLK,
	MX6Q_PAD_SD1_CMD__USDHC1_CMD,
	MX6Q_PAD_SD1_DAT0__USDHC1_DAT0,
	MX6Q_PAD_SD1_DAT1__USDHC1_DAT1,
	MX6Q_PAD_SD1_DAT2__USDHC1_DAT2,
	MX6Q_PAD_SD1_DAT3__USDHC1_DAT3,
};

iomux_v3_cfg_t usdhc2_pads[] = {
	MX6Q_PAD_SD2_CLK__USDHC2_CLK,
	MX6Q_PAD_SD2_CMD__USDHC2_CMD,
	MX6Q_PAD_SD2_DAT0__USDHC2_DAT0,
	MX6Q_PAD_SD2_DAT1__USDHC2_DAT1,
	MX6Q_PAD_SD2_DAT2__USDHC2_DAT2,
	MX6Q_PAD_SD2_DAT3__USDHC2_DAT3,
};

iomux_v3_cfg_t usdhc3_pads[] = {
	MX6Q_PAD_SD3_CLK__USDHC3_CLK,
	MX6Q_PAD_SD3_CMD__USDHC3_CMD,
	MX6Q_PAD_SD3_DAT0__USDHC3_DAT0,
	MX6Q_PAD_SD3_DAT1__USDHC3_DAT1,
	MX6Q_PAD_SD3_DAT2__USDHC3_DAT2,
	MX6Q_PAD_SD3_DAT3__USDHC3_DAT3,
	MX6Q_PAD_SD3_DAT4__USDHC3_DAT4,
	MX6Q_PAD_SD3_DAT5__USDHC3_DAT5,
	MX6Q_PAD_SD3_DAT6__USDHC3_DAT6,
	MX6Q_PAD_SD3_DAT7__USDHC3_DAT7,
};

iomux_v3_cfg_t usdhc4_pads[] = {
	MX6Q_PAD_SD4_CLK__USDHC4_CLK,
	MX6Q_PAD_SD4_CMD__USDHC4_CMD,
	MX6Q_PAD_SD4_DAT0__USDHC4_DAT0,
	MX6Q_PAD_SD4_DAT1__USDHC4_DAT1,
	MX6Q_PAD_SD4_DAT2__USDHC4_DAT2,
	MX6Q_PAD_SD4_DAT3__USDHC4_DAT3,
	MX6Q_PAD_SD4_DAT4__USDHC4_DAT4,
	MX6Q_PAD_SD4_DAT5__USDHC4_DAT5,
	MX6Q_PAD_SD4_DAT6__USDHC4_DAT6,
	MX6Q_PAD_SD4_DAT7__USDHC4_DAT7,
};
#elif defined CONFIG_MX6DL
iomux_v3_cfg_t usdhc1_pads[] = {
	MX6DL_PAD_SD1_CLK__USDHC1_CLK,
	MX6DL_PAD_SD1_CMD__USDHC1_CMD,
	MX6DL_PAD_SD1_DAT0__USDHC1_DAT0,
	MX6DL_PAD_SD1_DAT1__USDHC1_DAT1,
	MX6DL_PAD_SD1_DAT2__USDHC1_DAT2,
	MX6DL_PAD_SD1_DAT3__USDHC1_DAT3,
};

iomux_v3_cfg_t usdhc2_pads[] = {
	MX6DL_PAD_SD2_CLK__USDHC2_CLK,
	MX6DL_PAD_SD2_CMD__USDHC2_CMD,
	MX6DL_PAD_SD2_DAT0__USDHC2_DAT0,
	MX6DL_PAD_SD2_DAT1__USDHC2_DAT1,
	MX6DL_PAD_SD2_DAT2__USDHC2_DAT2,
	MX6DL_PAD_SD2_DAT3__USDHC2_DAT3,
};

iomux_v3_cfg_t usdhc3_pads[] = {
	MX6DL_PAD_SD3_CLK__USDHC3_CLK,
	MX6DL_PAD_SD3_CMD__USDHC3_CMD,
	MX6DL_PAD_SD3_DAT0__USDHC3_DAT0,
	MX6DL_PAD_SD3_DAT1__USDHC3_DAT1,
	MX6DL_PAD_SD3_DAT2__USDHC3_DAT2,
	MX6DL_PAD_SD3_DAT3__USDHC3_DAT3,
	MX6DL_PAD_SD3_DAT4__USDHC3_DAT4,
	MX6DL_PAD_SD3_DAT5__USDHC3_DAT5,
	MX6DL_PAD_SD3_DAT6__USDHC3_DAT6,
	MX6DL_PAD_SD3_DAT7__USDHC3_DAT7,
};

iomux_v3_cfg_t usdhc4_pads[] = {
	MX6DL_PAD_SD4_CLK__USDHC4_CLK,
	MX6DL_PAD_SD4_CMD__USDHC4_CMD,
	MX6DL_PAD_SD4_DAT0__USDHC4_DAT0,
	MX6DL_PAD_SD4_DAT1__USDHC4_DAT1,
	MX6DL_PAD_SD4_DAT2__USDHC4_DAT2,
	MX6DL_PAD_SD4_DAT3__USDHC4_DAT3,
	MX6DL_PAD_SD4_DAT4__USDHC4_DAT4,
	MX6DL_PAD_SD4_DAT5__USDHC4_DAT5,
	MX6DL_PAD_SD4_DAT6__USDHC4_DAT6,
	MX6DL_PAD_SD4_DAT7__USDHC4_DAT7,
};
#endif

int usdhc_gpio_init(bd_t *bis)
{
	s32 status = 0;
	u32 index = 0;

	for (index = 0; index < CONFIG_SYS_FSL_USDHC_NUM;
		++index) {
		switch (index) {
		case 0:
			mxc_iomux_v3_setup_multiple_pads(usdhc1_pads,
				sizeof(usdhc1_pads) /
				sizeof(usdhc1_pads[0]));
			break;
		case 1:
			mxc_iomux_v3_setup_multiple_pads(usdhc2_pads,
				sizeof(usdhc2_pads) /
				sizeof(usdhc2_pads[0]));
			break;
		case 2:
			mxc_iomux_v3_setup_multiple_pads(usdhc3_pads,
				sizeof(usdhc3_pads) /
				sizeof(usdhc3_pads[0]));
			break;
		case 3:
			mxc_iomux_v3_setup_multiple_pads(usdhc4_pads,
				sizeof(usdhc4_pads) /
				sizeof(usdhc4_pads[0]));
			break;
		default:
			printf("Warning: you configured more USDHC controllers"
				"(%d) then supported by the board (%d)\n",
				index+1, CONFIG_SYS_FSL_USDHC_NUM);
			return status;
		}
		status |= fsl_esdhc_initialize(bis, &usdhc_cfg[index]);
	}

	return status;
}

int board_mmc_init(bd_t *bis)
{
	if (!usdhc_gpio_init(bis))
		return 0;
	else
		return -1;
}

#ifdef CONFIG_MXC_EPDC
#ifdef CONFIG_SPLASH_SCREEN
int setup_splash_img(void)
{
#ifdef CONFIG_SPLASH_IS_IN_MMC
	int mmc_dev = get_mmc_env_devno();
	ulong offset = CONFIG_SPLASH_IMG_OFFSET;
	ulong size = CONFIG_SPLASH_IMG_SIZE;
	ulong addr = 0;
	char *s = NULL;
	struct mmc *mmc = find_mmc_device(mmc_dev);
	uint blk_start, blk_cnt, n;

	s = getenv("splashimage");

	if (NULL == s) {
		puts("env splashimage not found!\n");
		return -1;
	}
	addr = simple_strtoul(s, NULL, 16);

	if (!mmc) {
		printf("MMC Device %d not found\n",
			mmc_dev);
		return -1;
	}

	if (mmc_init(mmc)) {
		puts("MMC init failed\n");
		return  -1;
	}

	blk_start = ALIGN(offset, mmc->read_bl_len) / mmc->read_bl_len;
	blk_cnt   = ALIGN(size, mmc->read_bl_len) / mmc->read_bl_len;
	n = mmc->block_dev.block_read(mmc_dev, blk_start,
					blk_cnt, (u_char *)addr);
	flush_cache((ulong)addr, blk_cnt * mmc->read_bl_len);

	return (n == blk_cnt) ? 0 : -1;
#endif

	return 0;
}
#endif

vidinfo_t panel_info = {
	.vl_refresh = 85,
	.vl_col = 800,
	.vl_row = 600,
	.vl_pixclock = 26666667,
	.vl_left_margin = 8,
	.vl_right_margin = 100,
	.vl_upper_margin = 4,
	.vl_lower_margin = 8,
	.vl_hsync = 4,
	.vl_vsync = 1,
	.vl_sync = 0,
	.vl_mode = 0,
	.vl_flag = 0,
	.vl_bpix = 3,
	cmap:0,
};

struct epdc_timing_params panel_timings = {
	.vscan_holdoff = 4,
	.sdoed_width = 10,
	.sdoed_delay = 20,
	.sdoez_width = 10,
	.sdoez_delay = 20,
	.gdclk_hp_offs = 419,
	.gdsp_offs = 20,
	.gdoe_offs = 0,
	.gdclk_offs = 5,
	.num_ce = 1,
};

static void setup_epdc_power(void)
{
	unsigned int reg;

	/* Setup epdc voltage */

	/* EIM_A17 - GPIO2[21] for PWR_GOOD status */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_A17__GPIO_2_21);

	/* EIM_D17 - GPIO3[17] for VCOM control */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D17__GPIO_3_17);

	/* Set as output */
	reg = readl(GPIO3_BASE_ADDR + GPIO_GDIR);
	reg |= (1 << 17);
	writel(reg, GPIO3_BASE_ADDR + GPIO_GDIR);

	/* EIM_D20 - GPIO3[20] for EPD PMIC WAKEUP */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D20__GPIO_3_20);
	/* Set as output */
	reg = readl(GPIO3_BASE_ADDR + GPIO_GDIR);
	reg |= (1 << 20);
	writel(reg, GPIO3_BASE_ADDR + GPIO_GDIR);

	/* EIM_A18 - GPIO2[20] for EPD PWR CTL0 */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_A18__GPIO_2_20);
	/* Set as output */
	reg = readl(GPIO2_BASE_ADDR + GPIO_GDIR);
	reg |= (1 << 20);
	writel(reg, GPIO2_BASE_ADDR + GPIO_GDIR);
}

void epdc_power_on()
{
	unsigned int reg;

	/* Set EPD_PWR_CTL0 to high - enable EINK_VDD (3.15) */
	reg = readl(GPIO2_BASE_ADDR + GPIO_DR);
	reg |= (1 << 20);
	writel(reg, GPIO2_BASE_ADDR + GPIO_DR);

	/* Set PMIC Wakeup to high - enable Display power */
	reg = readl(GPIO3_BASE_ADDR + GPIO_DR);
	reg |= (1 << 20);
	writel(reg, GPIO3_BASE_ADDR + GPIO_DR);

	/* Wait for PWRGOOD == 1 */
	while (1) {
		reg = readl(GPIO2_BASE_ADDR + GPIO_DR);
		if (!(reg & (1 << 21)))
			break;

		udelay(100);
	}

	/* Enable VCOM */
	reg = readl(GPIO3_BASE_ADDR + GPIO_DR);
	reg |= (1 << 17);
	writel(reg, GPIO3_BASE_ADDR + GPIO_DR);

	reg = readl(GPIO3_BASE_ADDR + GPIO_DR);

	udelay(500);
}

void  epdc_power_off()
{
	unsigned int reg;
	/* Set PMIC Wakeup to low - disable Display power */
	reg = readl(GPIO3_BASE_ADDR + GPIO_DR);
	reg &= ~(1 << 20);
	writel(reg, GPIO3_BASE_ADDR + GPIO_DR);

	/* Disable VCOM */
	reg = readl(GPIO3_BASE_ADDR + GPIO_DR);
	reg &= ~(1 << 17);
	writel(reg, GPIO3_BASE_ADDR + GPIO_DR);

	/* Set EPD_PWR_CTL0 to low - disable EINK_VDD (3.15) */
	reg = readl(GPIO2_BASE_ADDR + GPIO_DR);
	reg &= ~(1 << 20);
	writel(reg, GPIO2_BASE_ADDR + GPIO_DR);
}

int setup_waveform_file()
{
#ifdef CONFIG_WAVEFORM_FILE_IN_MMC
	int mmc_dev = get_mmc_env_devno();
	ulong offset = CONFIG_WAVEFORM_FILE_OFFSET;
	ulong size = CONFIG_WAVEFORM_FILE_SIZE;
	ulong addr = CONFIG_WAVEFORM_BUF_ADDR;
	char *s = NULL;
	struct mmc *mmc = find_mmc_device(mmc_dev);
	uint blk_start, blk_cnt, n;

	if (!mmc) {
		printf("MMC Device %d not found\n",
			mmc_dev);
		return -1;
	}

	if (mmc_init(mmc)) {
		puts("MMC init failed\n");
		return -1;
	}

	blk_start = ALIGN(offset, mmc->read_bl_len) / mmc->read_bl_len;
	blk_cnt   = ALIGN(size, mmc->read_bl_len) / mmc->read_bl_len;
	n = mmc->block_dev.block_read(mmc_dev, blk_start,
		blk_cnt, (u_char *)addr);
	flush_cache((ulong)addr, blk_cnt * mmc->read_bl_len);

	return (n == blk_cnt) ? 0 : -1;
#else
	return -1;
#endif
}

static void setup_epdc()
{
	unsigned int reg;

	/* epdc iomux settings */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_A16__EPDC_SDDO_0);
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_DA10__EPDC_SDDO_1);
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_DA12__EPDC_SDDO_2);
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_DA11__EPDC_SDDO_3);
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_LBA__EPDC_SDDO_4);
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_EB2__EPDC_SDDO_5);
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_CS0__EPDC_SDDO_6);
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_RW__EPDC_SDDO_7);
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_A21__EPDC_GDCLK);
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_A22__EPDC_GDSP);
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_A23__EPDC_GDOE);
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_A24__EPDC_GDRL);
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D31__EPDC_SDCLK);
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D27__EPDC_SDOE);
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_DA1__EPDC_SDLE);
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_EB1__EPDC_SDSHR);
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_DA2__EPDC_BDR_0);
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_DA4__EPDC_SDCE_0);
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_DA5__EPDC_SDCE_1);
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_DA6__EPDC_SDCE_2);

	/*** epdc Maxim PMIC settings ***/

	/* EPDC PWRSTAT - GPIO2[21] for PWR_GOOD status */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_A17__GPIO_2_21);

	/* EPDC VCOM0 - GPIO3[17] for VCOM control */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D17__GPIO_3_17);

	/* UART4 TXD - GPIO3[20] for EPD PMIC WAKEUP */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D20__GPIO_3_20);

	/* EIM_A18 - GPIO2[20] for EPD PWR CTL0 */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_A18__GPIO_2_20);

	/*** Set pixel clock rates for EPDC ***/

	/* EPDC AXI clk (IPU2_CLK) from PFD_400M, set to 396/2 = 198MHz */
	reg = readl(CCM_BASE_ADDR + CLKCTL_CSCDR3);
	reg &= ~0x7C000;
	reg |= (1 << 16) | (1 << 14);
	writel(reg, CCM_BASE_ADDR + CLKCTL_CSCDR3);

	/* EPDC AXI clk enable */
	reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR3);
	reg |= 0x00C0;
	writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR3);

	/* EPDC PIX clk (IPU2_DI1_CLK) from PLL5, set to 650/4/6 = ~27MHz */
	reg = readl(CCM_BASE_ADDR + CLKCTL_CSCDR2);
	reg &= ~0x3FE00;
	reg |= (2 << 15) | (5 << 12);
	writel(reg, CCM_BASE_ADDR + CLKCTL_CSCDR2);

	/* PLL5 enable (defaults to 650) */
	reg = readl(ANATOP_BASE_ADDR + ANATOP_PLL_VIDEO);
	reg &= ~((1 << 16) | (1 << 12));
	reg |= (1 << 13);
	writel(reg, ANATOP_BASE_ADDR + ANATOP_PLL_VIDEO);

	/* EPDC PIX clk enable */
	reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR3);
	reg |= 0x0C00;
	writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR3);

	panel_info.epdc_data.working_buf_addr = CONFIG_WORKING_BUF_ADDR;
	panel_info.epdc_data.waveform_buf_addr = CONFIG_WAVEFORM_BUF_ADDR;

	panel_info.epdc_data.wv_modes.mode_init = 0;
	panel_info.epdc_data.wv_modes.mode_du = 1;
	panel_info.epdc_data.wv_modes.mode_gc4 = 3;
	panel_info.epdc_data.wv_modes.mode_gc8 = 2;
	panel_info.epdc_data.wv_modes.mode_gc16 = 2;
	panel_info.epdc_data.wv_modes.mode_gc32 = 2;

	panel_info.epdc_data.epdc_timings = panel_timings;

	setup_epdc_power();

	/* Assign fb_base */
	gd->fb_base = CONFIG_FB_BASE;
}
#endif

/* For DDR mode operation, provide target delay parameter for each SD port.
 * Use cfg->esdhc_base to distinguish the SD port #. The delay for each port
 * is dependent on signal layout for that particular port.  If the following
 * CONFIG is not defined, then the default target delay value will be used.
 */
#ifdef CONFIG_GET_DDR_TARGET_DELAY
u32 get_ddr_delay(struct fsl_esdhc_cfg *cfg)
{
	/* No delay required on SABRESD board SD ports */
	return 0;
}
#endif

#endif

#ifndef CONFIG_MXC_EPDC
#ifdef CONFIG_LCD
void lcd_enable(void)
{
	char *s;
	int ret;
	unsigned int reg;

	s = getenv("lvds_num");
	di = simple_strtol(s, NULL, 10);

	/*
	* hw_rev 2: IPUV3DEX
	* hw_rev 3: IPUV3M
	* hw_rev 4: IPUV3H
	*/
	g_ipu_hw_rev = IPUV3_HW_REV_IPUV3H;

	imx_pwm_config(pwm0, 25000, 50000);
	imx_pwm_enable(pwm0);

#if defined CONFIG_MX6Q
	/* PWM backlight */
	mxc_iomux_v3_setup_pad(MX6Q_PAD_SD1_DAT3__PWM1_PWMO);
	/* LVDS panel CABC_EN0 */
	mxc_iomux_v3_setup_pad(MX6Q_PAD_NANDF_CS2__GPIO_6_15);
	/* LVDS panel CABC_EN1 */
	mxc_iomux_v3_setup_pad(MX6Q_PAD_NANDF_CS3__GPIO_6_16);
#elif defined CONFIG_MX6DL
	/* PWM backlight */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_SD1_DAT3__PWM1_PWMO);
	/* LVDS panel CABC_EN0 */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_NANDF_CS2__GPIO_6_15);
	/* LVDS panel CABC_EN1 */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_NANDF_CS3__GPIO_6_16);
#endif
	/*
	 * Set LVDS panel CABC_EN0 to low to disable
	 * CABC function. This function will turn backlight
	 * automatically according to display content, so
	 * simply disable it to get rid of annoying unstable
	 * backlight phenomena.
	 */
	reg = readl(GPIO6_BASE_ADDR + GPIO_GDIR);
	reg |= (1 << 15);
	writel(reg, GPIO6_BASE_ADDR + GPIO_GDIR);

	reg = readl(GPIO6_BASE_ADDR + GPIO_DR);
	reg &= ~(1 << 15);
	writel(reg, GPIO6_BASE_ADDR + GPIO_DR);

	/*
	 * Set LVDS panel CABC_EN1 to low to disable
	 * CABC function.
	 */
	reg = readl(GPIO6_BASE_ADDR + GPIO_GDIR);
	reg |= (1 << 16);
	writel(reg, GPIO6_BASE_ADDR + GPIO_GDIR);

	reg = readl(GPIO6_BASE_ADDR + GPIO_DR);
	reg &= ~(1 << 16);
	writel(reg, GPIO6_BASE_ADDR + GPIO_DR);

	/* Disable ipu1_clk/ipu1_di_clk_x/ldb_dix_clk. */
	reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR3);
	reg &= ~0xC033;
	writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR3);

#if defined CONFIG_MX6Q
	/*
	 * Align IPU1 HSP clock and IPU1 DIx pixel clock
	 * with kernel setting to avoid screen flick when
	 * booting into kernel. Developer should change
	 * the relevant setting if kernel setting changes.
	 * IPU1 HSP clock tree:
	 * osc_clk(24M)->pll2_528_bus_main_clk(528M)->
	 * periph_clk(528M)->mmdc_ch0_axi_clk(528M)->
	 * ipu1_clk(264M)
	 */
	/* pll2_528_bus_main_clk */
	/* divider */
	writel(0x1, ANATOP_BASE_ADDR + 0x34);

	/* periph_clk */
	/* source */
	reg = readl(CCM_BASE_ADDR + CLKCTL_CBCMR);
	reg &= ~(0x3 << 18);
	writel(reg, CCM_BASE_ADDR + CLKCTL_CBCMR);

	reg = readl(CCM_BASE_ADDR + CLKCTL_CBCDR);
	reg &= ~(0x1 << 25);
	writel(reg, CCM_BASE_ADDR + CLKCTL_CBCDR);

	/*
	 * Check PERIPH_CLK_SEL_BUSY in
	 * MXC_CCM_CDHIPR register.
	 */
	do {
		udelay(5);
		reg = readl(CCM_BASE_ADDR + CLKCTL_CDHIPR);
	} while (reg & (0x1 << 5));

	/* mmdc_ch0_axi_clk */
	/* divider */
	reg = readl(CCM_BASE_ADDR + CLKCTL_CBCDR);
	reg &= ~(0x7 << 19);
	writel(reg, CCM_BASE_ADDR + CLKCTL_CBCDR);

	/*
	 * Check MMDC_CH0PODF_BUSY in
	 * MXC_CCM_CDHIPR register.
	 */
	do {
		udelay(5);
		reg = readl(CCM_BASE_ADDR + CLKCTL_CDHIPR);
	} while (reg & (0x1 << 4));

	/* ipu1_clk */
	reg = readl(CCM_BASE_ADDR + CLKCTL_CSCDR3);
	/* source */
	reg &= ~(0x3 << 9);
	/* divider */
	reg &= ~(0x7 << 11);
	reg |= (0x1 << 11);
	writel(reg, CCM_BASE_ADDR + CLKCTL_CSCDR3);

	/*
	 * ipu1_pixel_clk_x clock tree:
	 * osc_clk(24M)->pll2_528_bus_main_clk(528M)->
	 * pll2_pfd_352M(452.57M)->ldb_dix_clk(64.65M)->
	 * ipu1_di_clk_x(64.65M)->ipu1_pixel_clk_x(64.65M)
	 */
	/* pll2_pfd_352M */
	/* disable */
	writel(0x1 << 7, ANATOP_BASE_ADDR + 0x104);
	/* divider */
	writel(0x3F, ANATOP_BASE_ADDR + 0x108);
	writel(0x15, ANATOP_BASE_ADDR + 0x104);

	/* ldb_dix_clk */
	/* source */
	reg = readl(CCM_BASE_ADDR + CLKCTL_CS2CDR);
	reg &= ~(0x3F << 9);
	reg |= (0x9 << 9);
	writel(reg, CCM_BASE_ADDR + CLKCTL_CS2CDR);
	/* divider */
	reg = readl(CCM_BASE_ADDR + CLKCTL_CSCMR2);
	reg |= (0x3 << 10);
	writel(reg, CCM_BASE_ADDR + CLKCTL_CSCMR2);

	/* pll2_pfd_352M */
	/* enable after ldb_dix_clk source is set */
	writel(0x1 << 7, ANATOP_BASE_ADDR + 0x108);

	/* ipu1_di_clk_x */
	/* source */
	reg = readl(CCM_BASE_ADDR + CLKCTL_CHSCCDR);
	reg &= ~0xE07;
	reg |= 0x803;
	writel(reg, CCM_BASE_ADDR + CLKCTL_CHSCCDR);
#elif defined CONFIG_MX6DL /* CONFIG_MX6Q */
	/*
	 * IPU1 HSP clock tree:
	 * osc_clk(24M)->pll3_usb_otg_main_clk(480M)->
	 * pll3_pfd_540M(540M)->ipu1_clk(270M)
	 */
	/* pll3_usb_otg_main_clk */
	/* divider */
	writel(0x3, ANATOP_BASE_ADDR + 0x18);

	/* pll3_pfd_540M */
	/* divider */
	writel(0x3F << 8, ANATOP_BASE_ADDR + 0xF8);
	writel(0x10 << 8, ANATOP_BASE_ADDR + 0xF4);
	/* enable */
	writel(0x1 << 15, ANATOP_BASE_ADDR + 0xF8);

	/* ipu1_clk */
	reg = readl(CCM_BASE_ADDR + CLKCTL_CSCDR3);
	/* source */
	reg |= (0x3 << 9);
	/* divider */
	reg &= ~(0x7 << 11);
	reg |= (0x1 << 11);
	writel(reg, CCM_BASE_ADDR + CLKCTL_CSCDR3);

	/*
	 * ipu1_pixel_clk_x clock tree:
	 * osc_clk(24M)->pll2_528_bus_main_clk(528M)->
	 * pll2_pfd_352M(452.57M)->ldb_dix_clk(64.65M)->
	 * ipu1_di_clk_x(64.65M)->ipu1_pixel_clk_x(64.65M)
	 */
	/* pll2_528_bus_main_clk */
	/* divider */
	writel(0x1, ANATOP_BASE_ADDR + 0x34);

	/* pll2_pfd_352M */
	/* disable */
	writel(0x1 << 7, ANATOP_BASE_ADDR + 0x104);
	/* divider */
	writel(0x3F, ANATOP_BASE_ADDR + 0x108);
	writel(0x15, ANATOP_BASE_ADDR + 0x104);

	/* ldb_dix_clk */
	/* source */
	reg = readl(CCM_BASE_ADDR + CLKCTL_CS2CDR);
	reg &= ~(0x3F << 9);
	reg |= (0x9 << 9);
	writel(reg, CCM_BASE_ADDR + CLKCTL_CS2CDR);
	/* divider */
	reg = readl(CCM_BASE_ADDR + CLKCTL_CSCMR2);
	reg |= (0x3 << 10);
	writel(reg, CCM_BASE_ADDR + CLKCTL_CSCMR2);

	/* pll2_pfd_352M */
	/* enable after ldb_dix_clk source is set */
	writel(0x1 << 7, ANATOP_BASE_ADDR + 0x108);

	/* ipu1_di_clk_x */
	/* source */
	reg = readl(CCM_BASE_ADDR + CLKCTL_CHSCCDR);
	reg &= ~0xE07;
	reg |= 0x803;
	writel(reg, CCM_BASE_ADDR + CLKCTL_CHSCCDR);
#endif	/* CONFIG_MX6DL */

	/* Enable ipu1/ipu1_dix/ldb_dix clocks. */
	if (di == 1) {
		reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR3);
		reg |= 0xC033;
		writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR3);
	} else {
		reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR3);
		reg |= 0x300F;
		writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR3);
	}

	ret = ipuv3_fb_init(&lvds_xga, di, IPU_PIX_FMT_RGB666,
			DI_PCLK_LDB, 65000000);
	if (ret)
		puts("LCD cannot be configured\n");

	/*
	 * LVDS0 mux to IPU1 DI0.
	 * LVDS1 mux to IPU1 DI1.
	 */
	reg = readl(IOMUXC_BASE_ADDR + 0xC);
	reg &= ~(0x000003C0);
	reg |= 0x00000100;
	writel(reg, IOMUXC_BASE_ADDR + 0xC);

	if (di == 1)
		writel(0x40C, IOMUXC_BASE_ADDR + 0x8);
	else
		writel(0x201, IOMUXC_BASE_ADDR + 0x8);
}
#endif

#ifdef CONFIG_VIDEO_MX5
void panel_info_init(void)
{
	panel_info.vl_bpix = LCD_BPP;
	panel_info.vl_col = lvds_xga.xres;
	panel_info.vl_row = lvds_xga.yres;
	panel_info.cmap = colormap;
}
#endif

#ifdef CONFIG_SPLASH_SCREEN
void setup_splash_image(void)
{
	char *s;
	ulong addr;

	s = getenv("splashimage");

	if (s != NULL) {
		addr = simple_strtoul(s, NULL, 16);

#if defined(CONFIG_ARCH_MMU)
		addr = ioremap_nocache(iomem_to_phys(addr),
				fsl_bmp_reversed_600x400_size);
#endif
		memcpy((char *)addr, (char *)fsl_bmp_reversed_600x400,
				fsl_bmp_reversed_600x400_size);
	}
}
#endif
#endif /* !CONFIG_MXC_EPDC */

int board_init(void)
{
/* need set Power Supply Glitch to 0x41736166
*and need clear Power supply Glitch Detect bit
* when POR or reboot or power on Otherwise system
*could not be power off anymore*/
	u32 reg;
// inizio parte commentata
	writel(0x41736166, SNVS_BASE_ADDR + 0x64);/*set LPPGDR*/
	udelay(10);
	reg = readl(SNVS_BASE_ADDR + 0x4c);
	reg |= (1 << 3);
	writel(reg, SNVS_BASE_ADDR + 0x4c);/*clear LPSR*/
 // fine parte commentata

	mxc_iomux_v3_init((void *)IOMUXC_BASE_ADDR);

#if defined (UQSEVEN) || defined (uSBC)
        /* NANDF_D4 GPIO2[4] for UQSEVEN Boot Validate GPIO */
#ifdef CONFIG_MX6Q
	mxc_iomux_v3_setup_pad(MX6Q_PAD_NANDF_D4__GPIO_2_4);
#elif defined CONFIG_MX6DL
	mxc_iomux_v3_setup_pad(MX6DL_PAD_NANDF_D4__GPIO_2_4);
#endif
	/* Set as output */
	reg = readl(GPIO2_BASE_ADDR + GPIO_GDIR);
	reg |= (1 << 4);
	writel(reg, GPIO2_BASE_ADDR + GPIO_GDIR);

        /* Set Low */
        reg = readl(GPIO2_BASE_ADDR + GPIO_DR);
	reg &= ~(1 << 4);
	writel(reg, GPIO2_BASE_ADDR + GPIO_DR);
        
        udelay(1000);
        
        /* Set High */
        reg = readl(GPIO2_BASE_ADDR + GPIO_DR);
	reg |= (1 << 4);
	writel(reg, GPIO2_BASE_ADDR + GPIO_DR);
#elif defined QSEVEN

	/* boot validate */
#ifdef CONFIG_MX6Q
	mxc_iomux_v3_setup_pad(MX6Q_PAD_SD4_DAT5__GPIO_2_13);
#elif defined CONFIG_MX6DL
	mxc_iomux_v3_setup_pad(MX6DL_PAD_SD4_DAT5__GPIO_2_13);
#endif
	reg = readl(GPIO2_BASE_ADDR + GPIO_GDIR);
	reg |= (1 << 13);
	writel(reg, GPIO2_BASE_ADDR + GPIO_GDIR);

        /* Set Low */
	reg = readl(GPIO2_BASE_ADDR + GPIO_DR);
	reg &= ~(1 << 13);
	writel(reg, GPIO2_BASE_ADDR + GPIO_DR);
        
    udelay(1000);
        
        /* Set High */
    reg = readl(GPIO2_BASE_ADDR + GPIO_DR);
	reg |= (1 << 13);
	writel(reg, GPIO2_BASE_ADDR + GPIO_DR);

#endif

	setup_boot_device();

	fsl_set_system_rev();

	/* board id for linux */
#if defined(CONFIG_MX6Q_SECO_Q7) || defined(CONFIG_MX6DL_SECO_Q7) || defined(CONFIG_MX6S_SECO_Q7)
	gd->bd->bi_arch_number = MACH_TYPE_MX6_SECO_Q7;
#elif defined(CONFIG_MX6Q_SECO_UQ7) || defined(CONFIG_MX6DL_SECO_UQ7) || defined(CONFIG_MX6S_SECO_UQ7)
	gd->bd->bi_arch_number = MACH_TYPE_MX6_SECO_UQ7;
#elif defined(CONFIG_MX6Q_SECO_uSBC) || defined(CONFIG_MX6DL_SECO_uSBC) || defined(CONFIG_MX6S_SECO_uSBC)
	gd->bd->bi_arch_number = MACH_TYPE_MX6_SECO_uSBC;
#else
	gd->bd->bi_arch_number = MACH_TYPE_MX6Q_SECO_Q7;
#endif

	/* address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM_1 + 0x100;

	setup_uart();

#ifdef CONFIG_DWC_AHSATA
	if (cpu_is_mx6q())
		setup_sata();
#endif

#ifdef CONFIG_VIDEO_MX5
	/* Enable lvds power */
	setup_lvds_poweron();

	panel_info_init();

	gd->fb_base = CONFIG_FB_BASE;
#ifdef CONFIG_ARCH_MMU
	gd->fb_base = ioremap_nocache(iomem_to_phys(gd->fb_base), 0);
#endif
#endif

#ifdef CONFIG_NAND_GPMI
	setup_gpmi_nand();
#endif

#ifdef CONFIG_MXC_EPDC
	setup_epdc();
#endif
	return 0;
}


#ifdef CONFIG_ANDROID_RECOVERY

int check_recovery_cmd_file(void)
{
	int button_pressed = 0;
	int recovery_mode = 0;

	recovery_mode = check_and_clean_recovery_flag();

	/* Check Recovery Combo Button press or not. */
	mxc_iomux_v3_setup_pad(MX6X_IOMUX(PAD_GPIO_5__GPIO_1_5));

	gpio_direction_input(GPIO_VOL_DN_KEY);

	if (gpio_get_value(GPIO_VOL_DN_KEY) == 0) { /* VOL_DN key is low assert */
		button_pressed = 1;
		printf("Recovery key pressed\n");
	}

	return recovery_mode || button_pressed;
}
#endif

int board_late_init(void)
{
	int ret = 0;
	unsigned char boardrev[2];
	#ifdef CONFIG_I2C_MXC
	setup_i2c(CONFIG_SYS_I2C_PORT);
	i2c_bus_recovery();
	/* SECO BOARD REV */
	i2c_probe (0x40);
	if (i2c_read (0x40, 0x0E, 1, boardrev, 2)) {
		printf ("Read Boar Revision Failed\n");
	} else {
		gd->bd->board_revision[0] = boardrev[1];
		gd->bd->board_revision[1] = boardrev[0];
	}
	if (i2c_read (0x40, 0x0C, 1, boardrev, 2)) {
		printf ("Read Boar Revision Failed\n");
	} else {
		gd->bd->board_revision[2] = boardrev[1];
		gd->bd->board_revision[3] = boardrev[0];
	}
	switch (get_boot_device())
	{
#if defined(QSEVEN)
		case uSD_BOOT:		setenv("bootdev", "mmc3"); break;
		case EXT_SD_BOOT:	setenv("bootdev", "mmc0"); break;
		case eMMC_BOOT:		setenv("bootdev", "mmc2"); break;
		case USBOTG_BOOT:	setenv("bootdev", "usbotg");
							puts ("In USBOTG mode, using only default environment\n\n");
							set_default_env();
							break;
#elif defined(UQSEVEN)
		case EXT_SD_BOOT:	setenv("bootdev", "mmc3"); break;
		case eMMC_BOOT:		setenv("bootdev", "mmc2"); break;
		case USBOTG_BOOT:	setenv("bootdev", "usbotg");
							puts ("In USBOTG mode, using only default environment\n\n");
							set_default_env();
							break;
#elif defined(uSBC)
		case eMMC_BOOT:		setenv("bootdev", "mmc2"); break;
		case USBOTG_BOOT:	setenv("bootdev", "usbotg");
							puts ("In USBOTG mode, using only default environment\n\n");
							set_default_env();
							break;
#endif
		default:
			printf ("NO Boot Device to set\n");
			break;
	}
	ret = setup_pmic_voltages();
	if (ret)
		return -1;
	#endif
	return 0;
}

#ifdef CONFIG_MXC_FEC
static int phy_read(char *devname, unsigned char addr, unsigned char reg,
		    unsigned short *pdata)
{
	int ret = miiphy_read(devname, addr, reg, pdata);
	if (ret)
		printf("Error reading from %s PHY addr=%02x reg=%02x\n",
		       devname, addr, reg);
	return ret;
}

static int phy_write(char *devname, unsigned char addr, unsigned char reg,
		     unsigned short value)
{
	int ret = miiphy_write(devname, addr, reg, value);
	if (ret)
		printf("Error writing to %s PHY addr=%02x reg=%02x\n", devname,
		       addr, reg);
	return ret;
}

int mx6_rgmii_rework(char *devname, int phy_addr)
{
	unsigned short val;

	/* To enable AR8031 ouput a 125MHz clk from CLK_25M */
	phy_write(devname, phy_addr, 0xd, 0x7);
	phy_write(devname, phy_addr, 0xe, 0x8016);
	phy_write(devname, phy_addr, 0xd, 0x4007);
	phy_read(devname, phy_addr, 0xe, &val);

	val &= 0xffe3;
	val |= 0x18;
	phy_write(devname, phy_addr, 0xe, val);

	/* introduce tx clock delay */
	phy_write(devname, phy_addr, 0x1d, 0x5);
	phy_read(devname, phy_addr, 0x1e, &val);
	val |= 0x0100;
	phy_write(devname, phy_addr, 0x1e, val);

	return 0;
}

#if defined CONFIG_MX6Q
iomux_v3_cfg_t enet_pads[] = {
	MX6Q_PAD_ENET_MDIO__ENET_MDIO,
	MX6Q_PAD_ENET_MDC__ENET_MDC,
	MX6Q_PAD_RGMII_TXC__ENET_RGMII_TXC,
	MX6Q_PAD_RGMII_TD0__ENET_RGMII_TD0,
	MX6Q_PAD_RGMII_TD1__ENET_RGMII_TD1,
	MX6Q_PAD_RGMII_TD2__ENET_RGMII_TD2,
	MX6Q_PAD_RGMII_TD3__ENET_RGMII_TD3,
	MX6Q_PAD_RGMII_TX_CTL__ENET_RGMII_TX_CTL,
	MX6Q_PAD_ENET_REF_CLK__ENET_TX_CLK,
	MX6Q_PAD_RGMII_RXC__ENET_RGMII_RXC,
	MX6Q_PAD_RGMII_RD0__ENET_RGMII_RD0,
	MX6Q_PAD_RGMII_RD1__ENET_RGMII_RD1,
	MX6Q_PAD_RGMII_RD2__ENET_RGMII_RD2,
	MX6Q_PAD_RGMII_RD3__ENET_RGMII_RD3,
	MX6Q_PAD_RGMII_RX_CTL__ENET_RGMII_RX_CTL,
	MX6Q_PAD_GPIO_0__CCM_CLKO,
	MX6Q_PAD_GPIO_3__CCM_CLKO2,
};
#elif defined CONFIG_MX6DL
iomux_v3_cfg_t enet_pads[] = {
	MX6DL_PAD_ENET_MDIO__ENET_MDIO,
	MX6DL_PAD_ENET_MDC__ENET_MDC,
	MX6DL_PAD_RGMII_TXC__ENET_RGMII_TXC,
	MX6DL_PAD_RGMII_TD0__ENET_RGMII_TD0,
	MX6DL_PAD_RGMII_TD1__ENET_RGMII_TD1,
	MX6DL_PAD_RGMII_TD2__ENET_RGMII_TD2,
	MX6DL_PAD_RGMII_TD3__ENET_RGMII_TD3,
	MX6DL_PAD_RGMII_TX_CTL__ENET_RGMII_TX_CTL,
	MX6DL_PAD_ENET_REF_CLK__ENET_TX_CLK,
	MX6DL_PAD_RGMII_RXC__ENET_RGMII_RXC,
	MX6DL_PAD_RGMII_RD0__ENET_RGMII_RD0,
	MX6DL_PAD_RGMII_RD1__ENET_RGMII_RD1,
	MX6DL_PAD_RGMII_RD2__ENET_RGMII_RD2,
	MX6DL_PAD_RGMII_RD3__ENET_RGMII_RD3,
	MX6DL_PAD_RGMII_RX_CTL__ENET_RGMII_RX_CTL,
	MX6DL_PAD_GPIO_0__CCM_CLKO,
};
#endif

void enet_board_init(void)
{
	unsigned int reg;
#if defined CONFIG_MX6Q
	iomux_v3_cfg_t enet_reset =
			(_MX6Q_PAD_ENET_CRS_DV__GPIO_1_25 &
			~MUX_PAD_CTRL_MASK)           |
			 MUX_PAD_CTRL(0x88);
#elif defined CONFIG_MX6DL
	iomux_v3_cfg_t enet_reset =
			(MX6DL_PAD_ENET_CRS_DV__GPIO_1_25 &
			~MUX_PAD_CTRL_MASK)           |
			 MUX_PAD_CTRL(0x88);
#endif

	mxc_iomux_v3_setup_multiple_pads(enet_pads,
			ARRAY_SIZE(enet_pads));
	mxc_iomux_v3_setup_pad(enet_reset);

	/* phy reset: gpio1-25 */
	reg = readl(GPIO1_BASE_ADDR + 0x0);
	reg &= ~0x2000000;
	writel(reg, GPIO1_BASE_ADDR + 0x0);

	reg = readl(GPIO1_BASE_ADDR + 0x4);
	reg |= 0x2000000;
	writel(reg, GPIO1_BASE_ADDR + 0x4);

	udelay(500);

	reg = readl(GPIO1_BASE_ADDR + 0x0);
	reg |= 0x2000000;
	writel(reg, GPIO1_BASE_ADDR + 0x0);
}
#endif

int checkboard(void)
{
#ifdef  QSEVEN
	printf("Board: %s-QSEVEN: %s Board: 0x%x [",
	mx6_chip_name(),
	mx6_board_rev_name(),
	fsl_system_rev);
#endif
#ifdef  UQSEVEN
	printf("Board: %s-uQSEVEN: %s Board: 0x%x [",
	mx6_chip_name(),
	mx6_board_rev_name(),
	fsl_system_rev);

#endif
#ifdef  uSBC
        printf("Board: %s-uSBC: %s Board: 0x%x [",
        mx6_chip_name(),
        mx6_board_rev_name(),
        fsl_system_rev);

#endif

	switch (__REG(SRC_BASE_ADDR + 0x8)) {
	case 0x0001:
		printf("POR");
		break;
	case 0x0009:
		printf("RST");
		break;
	case 0x0010:
	case 0x0011:
		printf("WDOG");
		break;
	default:
		printf("unknown");
	}
	printf("]\n");

	printf("Boot Device: ");
	switch (get_boot_device()) {
	case WEIM_NOR_BOOT:
		printf("NOR\n");
		break;
	case ONE_NAND_BOOT:
		printf("ONE NAND\n");
		break;
	case PATA_BOOT:
		printf("PATA\n");
		break;
	case SATA_BOOT:
		printf("SATA\n");
		break;
	case I2C_BOOT:
		printf("I2C\n");
		break;
	case SPI_NOR_BOOT:
		printf("SPI NOR\n");
		break;
	case uSD_BOOT:
		printf("On board uSD\n");
		break;
	case EXT_SD_BOOT:
		printf("External SD\n");
		break;
	case eMMC_BOOT:
		printf("eMMC\n");
		break;
	case NAND_BOOT:
		printf("NAND\n");
		break;
	case USBOTG_BOOT:
		printf("USB OTG\n");
		break;
	case UNKNOWN_BOOT:
	default:
		printf("UNKNOWN\n");
		break;
	}

#ifdef CONFIG_SECURE_BOOT
	if (check_hab_enable() == 1)
		get_hab_status();
#endif

	return 0;
}

#ifdef CONFIG_IMX_UDC

void udc_pins_setting(void)
{
	mxc_iomux_v3_setup_pad(MX6X_IOMUX(PAD_ENET_RX_ER__ANATOP_USBOTG_ID));
	mxc_iomux_v3_setup_pad(MX6X_IOMUX(PAD_EIM_D22__GPIO_3_22));
	mxc_iomux_v3_setup_pad(MX6X_IOMUX(PAD_ENET_TXD1__GPIO_1_29));

	/* USB_OTG_PWR = 0 */
	gpio_direction_output(USB_OTG_PWR, 0);
	/* USB_H1_POWER = 1 */
	gpio_direction_output(USB_H1_POWER, 1);

	mxc_iomux_set_gpr_register(1, 13, 1, 0);

}
#endif

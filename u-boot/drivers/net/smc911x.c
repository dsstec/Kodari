/*
 * SMSC LAN9[12]1[567] Network driver
 *
 * (c) 2007 Pengutronix, Sascha Hauer <s.hauer@pengutronix.de>
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
#include <command.h>
#include <malloc.h>
#include <net.h>
#include <miiphy.h>

#include "smc911x.h"

u32 pkt_data_pull(struct eth_device *dev, u32 addr) \
	__attribute__ ((weak, alias ("smc911x_reg_read")));
void pkt_data_push(struct eth_device *dev, u32 addr, u32 val) \
	__attribute__ ((weak, alias ("smc911x_reg_write")));

#define mdelay(n)       udelay((n)*1000)

#if defined(CONFIG_S5P6450)
DECLARE_GLOBAL_DATA_PTR;
#endif

#define FEATURE_EEPROM_CONTROL
#define MAC_VENDOR	0x849000	// atech system: 0x009084
#define EEP_ADDR_CHECK_CNT	0x10  // EEPROM address: 0x10

#if defined(FEATURE_EEPROM_CONTROL)
static int smsc_ctrlc(void)
{
	return (tstc() && getc() == 0x03);
}

static int do_eeprom_cmd(struct eth_device *dev, int cmd, u8 reg)
{
	if (smc911x_reg_read(dev, E2P_CMD) & E2P_CMD_EPC_BUSY) {
		printf("eeprom_cmd: busy at start (E2P_CMD = 0x%08x)\n",
			smc911x_reg_read(dev, E2P_CMD));
		return -1;
	}

	smc911x_reg_write(dev, E2P_CMD, E2P_CMD_EPC_BUSY | cmd | reg);

	while (smc911x_reg_read(dev, E2P_CMD) & E2P_CMD_EPC_BUSY)
		if (smsc_ctrlc()) {
			printf("eeprom_cmd: timeout (E2P_CMD = 0x%08x)\n",
				smc911x_reg_read(dev, E2P_CMD));
			return -1;
		}

	return 0;
}
static int eeprom_write_protect(struct eth_device *dev, u8 reg)
{
	int ret=0;
	unsigned char addr;
	
	/* disable erasing/writing */
	for(addr=reg; addr < reg+5; addr++) {
		ret = do_eeprom_cmd(dev, E2P_CMD_EPC_CMD_EWDS, addr);
	}
	return ret;
}

static u8 read_eeprom_reg(struct eth_device *dev, u8 reg)
{
	int ret = do_eeprom_cmd(dev, E2P_CMD_EPC_CMD_READ, reg);
	return (ret ? : smc911x_reg_read(dev, E2P_DATA));
}

static int write_eeprom_reg(struct eth_device *dev, u8 value, u8 reg)
{
	int ret;

	/* enable erasing/writing */
	ret = do_eeprom_cmd(dev, E2P_CMD_EPC_CMD_EWEN, reg);
	if (ret)
		goto done;

	/* erase the eeprom reg */
	ret = do_eeprom_cmd(dev, E2P_CMD_EPC_CMD_ERASE, reg);
	if (ret)
		goto done;

	/* write the eeprom reg */
	smc911x_reg_write(dev, E2P_DATA, value);
	ret = do_eeprom_cmd(dev, E2P_CMD_EPC_CMD_WRITE, reg);
	if (ret)
		goto done;

	/* disable erasing/writing */
	ret = do_eeprom_cmd(dev, E2P_CMD_EPC_CMD_EWDS, reg);

 done:
	return ret;
}

static unsigned char read_eeprom_check_count(struct eth_device *dev)
{
	unsigned char val;
	val = read_eeprom_reg(dev, EEP_ADDR_CHECK_CNT);
	printf("Read EEP check count: %d\n",val);
	return val;
}
static void write_eeprom_check_count(struct eth_device *dev, unsigned char val)
{
	int ret;
	ret = write_eeprom_reg(dev, val, EEP_ADDR_CHECK_CNT);
	if (ret) {
		printf("eeprom writing error!!\n");
		return;
	}
	printf("Write EEP check count: %d\n",val);
}

static void read_all_eeprom(struct eth_device *dev)
{
	int i,j;
	unsigned char data;

	printf("\n----------------- Read EEPROM : 93AA46C----------------");
	/* 93AA46C : EEPROM 128byte, Address: 0x00 ~ 0x7F  */
	for(i=0; i<8; i++) {
		printf("\n 0x%03x:",i*16);

		for(j=i*16; j<(i*16+16); j++) {
			data = read_eeprom_reg(dev, j);
			printf(" %02x",data);
		}
	}
	printf("\n---------------------- End ----------------------------\n");
}

static void copy_mac_from_eeprom(struct eth_device *dev)
{
	ulong addrl =
		read_eeprom_reg(dev, 0x01) |
		read_eeprom_reg(dev, 0x02) << 8 |
		read_eeprom_reg(dev, 0x03) << 16 |
		read_eeprom_reg(dev, 0x04) << 24;
	ulong addrh =
		read_eeprom_reg(dev, 0x05) |
		read_eeprom_reg(dev, 0x06) << 8;
	smc911x_set_mac_csr(dev, ADDRL, addrl);
	smc911x_set_mac_csr(dev, ADDRH, addrh);

	printf("[read EEPROM]addrl=0x%x addrh=0x%x\n", addrl,addrh);
}

static void write_mac_to_eeprom(struct eth_device *dev, unsigned char *data)
{
//	int ret;
//	unsigned long addrl, addrh;
	unsigned char *mac = data;
	unsigned char check_cnt;

	check_cnt = read_eeprom_reg(dev, EEP_ADDR_CHECK_CNT);
/*
	addrl = mac[0] | (mac[1] << 8) | (mac[2] << 16) | (mac[3] << 24);
	addrh = mac[4] | (mac[5] << 8);
	smc911x_set_mac_csr(dev, ADDRL, addrl);
	smc911x_set_mac_csr(dev, ADDRH, addrh);
*/
	/* mac csr read check ID: 0xA5 */
	write_eeprom_reg(dev, 0xA5, 0x00);

	/* MAC address */
	write_eeprom_reg(dev, mac[0], 0x01);
	write_eeprom_reg(dev, mac[1], 0x02);
	write_eeprom_reg(dev, mac[2], 0x03);
	write_eeprom_reg(dev, mac[3], 0x04);
	write_eeprom_reg(dev, mac[4], 0x05);
	write_eeprom_reg(dev, mac[5], 0x06);

	printf("Write EEPROM MAC is %pM\n", mac);

	check_cnt++;
	write_eeprom_reg(dev, check_cnt, EEP_ADDR_CHECK_CNT);
//	printf("Write MAC check_cnt: %d\n",check_cnt);
}
#endif /* FEATURE_EEPROM_CONTROL */

static void smc911x_handle_mac_address(struct eth_device *dev)
{
	unsigned long addrh, addrl;
	uchar *m = dev->enetaddr;

	addrl = m[0] | (m[1] << 8) | (m[2] << 16) | (m[3] << 24);
	addrh = m[4] | (m[5] << 8);
	smc911x_set_mac_csr(dev, ADDRL, addrl);
	smc911x_set_mac_csr(dev, ADDRH, addrh);

	printf(DRIVERNAME ": MAC %pM\n", m);
}

static int smc911x_miiphy_read(struct eth_device *dev,
				u8 phy, u8 reg, u16 *val)
{
	while (smc911x_get_mac_csr(dev, MII_ACC) & MII_ACC_MII_BUSY)
		;

	smc911x_set_mac_csr(dev, MII_ACC, phy << 11 | reg << 6 |
				MII_ACC_MII_BUSY);

	while (smc911x_get_mac_csr(dev, MII_ACC) & MII_ACC_MII_BUSY)
		;

	*val = smc911x_get_mac_csr(dev, MII_DATA);

	return 0;
}

static int smc911x_miiphy_write(struct eth_device *dev,
				u8 phy, u8 reg, u16  val)
{
	while (smc911x_get_mac_csr(dev, MII_ACC) & MII_ACC_MII_BUSY)
		;

	smc911x_set_mac_csr(dev, MII_DATA, val);
	smc911x_set_mac_csr(dev, MII_ACC,
		phy << 11 | reg << 6 | MII_ACC_MII_BUSY | MII_ACC_MII_WRITE);

	while (smc911x_get_mac_csr(dev, MII_ACC) & MII_ACC_MII_BUSY)
		;
	return 0;
}

static int smc911x_phy_reset(struct eth_device *dev)
{
	u32 reg;

	reg = smc911x_reg_read(dev, PMT_CTRL);
	reg &= ~0xfffff030;
	reg |= PMT_CTRL_PHY_RST;
	smc911x_reg_write(dev, PMT_CTRL, reg);

	mdelay(100);

	return 0;
}

static void smc911x_phy_configure(struct eth_device *dev)
{
	int timeout;
	u16 status;

	smc911x_phy_reset(dev);

	smc911x_miiphy_write(dev, 1, PHY_BMCR, PHY_BMCR_RESET);
	mdelay(1);
	smc911x_miiphy_write(dev, 1, PHY_ANAR, 0x01e1);
	smc911x_miiphy_write(dev, 1, PHY_BMCR, PHY_BMCR_AUTON |
				PHY_BMCR_RST_NEG);

	timeout = 5000;
	do {
		mdelay(1);
		if ((timeout--) == 0)
			goto err_out;

		if (smc911x_miiphy_read(dev, 1, PHY_BMSR, &status) != 0)
			goto err_out;
	} while (!(status & PHY_BMSR_LS));

	printf(DRIVERNAME ": phy initialized\n");

	return;

err_out:
	printf(DRIVERNAME ": autonegotiation timed out\n");
}

static void smc911x_enable(struct eth_device *dev)
{
	/* Enable TX */
	smc911x_reg_write(dev, HW_CFG, 8 << 16 | HW_CFG_SF);

	smc911x_reg_write(dev, GPT_CFG, GPT_CFG_TIMER_EN | 10000);

	smc911x_reg_write(dev, TX_CFG, TX_CFG_TX_ON);

	/* no padding to start of packets */
	smc911x_reg_write(dev, RX_CFG, 0);

	smc911x_set_mac_csr(dev, MAC_CR, MAC_CR_TXEN | MAC_CR_RXEN |
				MAC_CR_HBDIS);

}

static int smc911x_init(struct eth_device *dev, bd_t * bd)
{
	struct chip_id *id = dev->priv;

	printf(DRIVERNAME ": detected %s controller\n", id->name);

	smc911x_reset(dev);

	/* Configure the PHY, initialize the link state */
	smc911x_phy_configure(dev);

	smc911x_handle_mac_address(dev);

	/* Turn on Tx + Rx */
	smc911x_enable(dev);

	return 0;
}

static int smc911x_send(struct eth_device *dev,
			volatile void *packet, int length)
{
	u32 *data = (u32*)packet;
	u32 tmplen;
	u32 status;

	smc911x_reg_write(dev, TX_DATA_FIFO, TX_CMD_A_INT_FIRST_SEG |
				TX_CMD_A_INT_LAST_SEG | length);
	smc911x_reg_write(dev, TX_DATA_FIFO, length);

	tmplen = (length + 3) / 4;

	while (tmplen--)
		pkt_data_push(dev, TX_DATA_FIFO, *data++);

	/* wait for transmission */
	while (!((smc911x_reg_read(dev, TX_FIFO_INF) &
					TX_FIFO_INF_TSUSED) >> 16));

	/* get status. Ignore 'no carrier' error, it has no meaning for
	 * full duplex operation
	 */
	status = smc911x_reg_read(dev, TX_STATUS_FIFO) &
			(TX_STS_LOC | TX_STS_LATE_COLL | TX_STS_MANY_COLL |
			TX_STS_MANY_DEFER | TX_STS_UNDERRUN);

	if (!status)
		return 0;

	printf(DRIVERNAME ": failed to send packet: %s%s%s%s%s\n",
		status & TX_STS_LOC ? "TX_STS_LOC " : "",
		status & TX_STS_LATE_COLL ? "TX_STS_LATE_COLL " : "",
		status & TX_STS_MANY_COLL ? "TX_STS_MANY_COLL " : "",
		status & TX_STS_MANY_DEFER ? "TX_STS_MANY_DEFER " : "",
		status & TX_STS_UNDERRUN ? "TX_STS_UNDERRUN" : "");

	return -1;
}

static void smc911x_halt(struct eth_device *dev)
{
	smc911x_reset(dev);
}

static int smc911x_rx(struct eth_device *dev)
{
	u32 *data = (u32 *)NetRxPackets[0];
	u32 pktlen, tmplen;
	u32 status, tmp, i;

	/* workaround: delay for rx packet should be added here.
	 * because NetLoop does not guarantee the RX packet delay.
	 */
	for (i=0; i<0x100000; i++) {
		if ((smc911x_reg_read(dev, RX_FIFO_INF) & RX_FIFO_INF_RXSUSED) >> 16)
			break;
	}
	if (i > (0x100000-1)) {
		printf("%s: timeout in RX\n", DRIVERNAME);
		return -1;
	}

	if ((smc911x_reg_read(dev, RX_FIFO_INF) & RX_FIFO_INF_RXSUSED) >> 16) {
		status = smc911x_reg_read(dev, RX_STATUS_FIFO);
		pktlen = (status & RX_STS_PKT_LEN) >> 16;

		smc911x_reg_write(dev, RX_CFG, 0);

		tmplen = (pktlen + 3) / 4;
		while (tmplen--)
			*data++ = pkt_data_pull(dev, RX_DATA_FIFO);

		if (status & RX_STS_ES)
			printf(DRIVERNAME
				": dropped bad packet. Status: 0x%08x\n",
				status);
		else
			NetReceive(NetRxPackets[0], pktlen);
	}

	return 0;
}

int smc911x_initialize(u8 dev_num, int base_addr)
{
	unsigned long addrl, addrh;
	unsigned long envl, envh;
	struct eth_device *dev;
	char bootargs_buf[30];       /* kernel에 전달하는 ENV MAC address 저장 변수 */
	char ethaddr_buf[20];        /* ENV 'ethaddr' MAC address를 읽어서 저장하는 변수 */
	unsigned char envmac[6];     /* ENV MAC address를 읽어서 저장하는 임시 변수 */
	unsigned char eep_check_cnt; /* EEPROM에 MAC address를 write한 횟수 저장 */

	dev = malloc(sizeof(*dev));
	if (!dev) {
		return -1;
	}
	memset(dev, 0, sizeof(*dev));

	dev->iobase = base_addr;

	/* Try to detect chip. Will fail if not present. */
	if (smc911x_detect_chip(dev)) {
		free(dev);
		return 0;
	}

#if defined(FEATURE_EEPROM_CONTROL) 
	/* Read EEPROM check count */
	eep_check_cnt = read_eeprom_check_count(dev);
	
	/* EEPROM에 MAC address를 write한 횟수를 저장하는 count를 0으로 초기화 한다 */
	/* EEPROM이 비정상적으로 지워지는지 확인하기 위한 count 변수 */
	if(eep_check_cnt == 255) {
		write_eeprom_check_count(dev,0);
	}
	
	/* Write MAC Address to EEPROM manually */
	/* ROM Writer를 사용하지 않고 MAC Address를 EEPROM에 write하기 위해 추가한 루틴이다 */
	/* Boot mode에서 setenv eepmac "00:90:84:12:34:56"; saveenv 로 저장 후 재부팅하면 이 루틴을 실행한다 */
	if(eth_getenv_enetaddr("eepmac", envmac)) {
		printf("\nManual EEPROM write %pM\n", envmac);

		envl = envmac[0] | (envmac[1] << 8) | (envmac[2] << 16) | (envmac[3] << 24);
		envh = envmac[4] | (envmac[5] << 8);
		
		/* ENV 'eepmac'에 저장된 MAC address의 Vendor code(00:90:84)가 일치할때만 EEPOM에 write한다 */
		if ((envl&0xffffff) == MAC_VENDOR) {
			write_mac_to_eeprom(dev,envmac);
		}
		/* Initialize 'envmac' */
		memset(envmac,0xff,6);
		sprintf(ethaddr_buf, "%pM", envmac);
		setenv("eepmac",ethaddr_buf);
		saveenv();

		read_all_eeprom	(dev);
	}
#endif

	eth_getenv_enetaddr("ethaddr", envmac);
	printf("env MAC is    %pM\n", envmac);

	envl = envmac[0] | (envmac[1] << 8) | (envmac[2] << 16) | (envmac[3] << 24);
	envh = envmac[4] | (envmac[5] << 8);
	//printf("ENV:addrl=0x%x addrh=0x%x\n", envl,envh);

	addrh = smc911x_get_mac_csr(dev, ADDRH);
	addrl = smc911x_get_mac_csr(dev, ADDRL);
	printf("EEP:addrl=0x%x addrh=0x%x\n", addrl,addrh);
	if (!(addrl == 0xffffffff && addrh == 0x0000ffff)
		&& !(addrl == 0x00000000 && addrh == 0x00000000)
		&& ((addrl&0xffffff) == MAC_VENDOR)
	) {
		/* address is obtained from optional eeprom */
		dev->enetaddr[0] = addrl;
		dev->enetaddr[1] = addrl >>  8;
		dev->enetaddr[2] = addrl >> 16;
		dev->enetaddr[3] = addrl >> 24;
		dev->enetaddr[4] = addrh;
		dev->enetaddr[5] = addrh >> 8;

		printf("eeprom MAC is %pM\n", dev->enetaddr);

		/* EEPROM MAC address와 ENV 'ethaddr'의 MAC address가 다르면 ENV 변수에 MAC address를 복사한다 */		
		if (memcmp(dev->enetaddr, envmac, 6)) {
			printf("EEPROM MAC address don't match with EMMC\n");	
			sprintf(bootargs_buf, "ethaddr=%pM", dev->enetaddr);
			setenv("bootargs", bootargs_buf);

			sprintf(ethaddr_buf, "%pM", dev->enetaddr);
			setenv("ethaddr", ethaddr_buf);
			saveenv();
			printf("EEPROM MAC is copyed to EMMC\n");
		}
	}
	else {
#if defined(FEATURE_EEPROM_CONTROL)
		/* ENV 'ethaddr'에 저장된 MAC address의 Vendor code(00:90:84)가 일치할때만 EEPOM에 write한다 */
		if ((envl&0xffffff) == MAC_VENDOR) {
			printf("EEPROM MAC is restored from EMMC\n");	
			write_mac_to_eeprom(dev,envmac);
		}
#endif
	}

#if 0 //delete, prevent of erasing eeprom mac ?
/* 이 코드는 부트로더에서 Ethernet을 사용하지 않으면 필요없다 */
/* Car type에서는 이 코드가 수행되지 않기 때문에 HS type에서도 삭제한다 */
	dev->init = smc911x_init;
	dev->halt = smc911x_halt;
	dev->send = smc911x_send;
	dev->recv = smc911x_rx;
	sprintf(dev->name, "%s-%hu", DRIVERNAME, dev_num);

	eth_register(dev);
#endif

	return 1;
}

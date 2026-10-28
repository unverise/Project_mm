/*
 * FM1210 one wire driver!
 * Copyright (C) 2020 FMSH, Inc., All Rights Reserved.
 * fm1210_ow_gpio.c - FM1210 one wire device module. Requires low level 1-Wire connection.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/compat.h>
#include <asm/uaccess.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/ctype.h>
#include <linux/spinlock_types.h>
#include <linux/spinlock.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/of_platform.h>
#include "mtk_charger.h"
#include "fm1210_ow_gpio.h"
#include "core/fm1210_ecdsa_api.h"
#if IS_ENABLED(CONFIG_TRAN_TRANLOG)
#include "tranlog.h"
#endif

/************************************************************/

static int hm_error_cnt;
static int j;
static u8 ecc_sig_res[ECC_SIG_SIZE];
struct ow_tm_data ow_tm_para;
static u8 swi_protocol_para_index;
static u8 swi_protocol_hm_flag;

static const u16 ow_tm[5][11] = {
	//0:tm_trstL;
	//1:tm_trecRST;
	//2:tm_trec;
	//3:tm_tw1L;
	//4:tm_tw0L;
	//5:tm_trL;
	//6:tm_trgt;
	//7:tm_tpdL;
	//8:tm_tbytegt;
	//9:tm_tcmdgt;
	//10:tm_tsamp;
	{ 2500, 1000, 5,  4, 45,  4, 62,  50000, 50, 30, 10 },         //FM1210_50us
	{ 2500, 1000, 7,  4, 65,  4, 80,  50000, 70,  30, 10 },         //FM1210_70us
	{ 2500, 1000, 10, 4, 95,  4, 110, 50000, 100, 30, 10 },         //FM1210_100us
	{ 2500, 1000, 10, 4, 140, 4, 150, 50000, 150, 30, 10 },         //FM1210_150us
	{ 2500, 1000, 10, 4, 185, 4, 200, 50000, 200, 30, 10 },         //FM1210_200us
};

void ow_set_tm(struct ow_tm_data *para)
{
	memcpy(&ow_tm_para, para, sizeof(struct ow_tm_data));
}

void str_to_hex(char *pdest, const char *psrc, int ilen)
{
	char h1, h2;
	char s1, s2;
	int i;

	for (i = 0; i < ilen; i++) {
		h1 = psrc[2 * i];
		h2 = psrc[2 * i + 1];
		s1 = toupper(h1) - 0x30;
		if (s1 > 9)
			s1 -= 7;
		s2 = toupper(h2) - 0x30;
		if (s2 > 9)
			s2 -= 7;
		pdest[i] = s1 * 16 + s2;
	}
}

void hex_to_str(char *pdest, char *psrc, int ilen)
{
	char ddl, ddh;
	int i;

	for (i = 0; i < ilen; i++) {
		ddh = 48 + psrc[i] / 16;
		ddl = 48 + psrc[i] % 16;
		if (ddh > 57)
			ddh = ddh + 7;
		if (ddl > 57)
			ddl = ddl + 7;
		pdest[i * 2] = ddh;
		pdest[i * 2 + 1] = ddl;
	}
	pdest[ilen * 2] = '\0';
}

u16 be16p_to_cpu(u8 *p)
{
	return (u16)((p[0] << 8) | p[1]);
}

void dump_data(u8 *buf, u16 len)
{
	u16 i;

	for (i = 0; i < len; i++)
		pr_err("%02X ", buf[i]);
	pr_err("\n");
}

#if OW_DEBUG == 1
void ow_dbg_switch_gpio(u8 cnt)
{
	u32 i = 0;

	for (; i < cnt; i++) {
		ow_dbg_low();
		ow_dbg_high();
	}
}
#endif

/*********************************************/
static void write_bit(struct fm1210 *chip_ctx, u8 bit_data)
{
	u16 delay_twhgt = ow_tm_para.tm_tw0L - ow_tm_para.tm_tw1L;

	gpio_direction_output(chip_ctx->swi_gpio, LOW);
	if (bit_data) {
		udelay(ow_tm_para.tm_tw1L);
		gpio_set_value(chip_ctx->swi_gpio, HIGH);
		udelay(delay_twhgt);
	} else {
		udelay(ow_tm_para.tm_tw0L);
	}
	gpio_set_value(chip_ctx->swi_gpio, HIGH);
	udelay(ow_tm_para.tm_trec);
}

static u8 read_bit(struct fm1210 *chip_ctx)
{
	u8 bit_data = 1;
	u16 dly_read_end = ow_tm_para.tm_trgt - ow_tm_para.tm_tsamp;

	gpio_direction_output(chip_ctx->swi_gpio, LOW);
	ow_dbg_low();
	ow_dbg_high();
	udelay(ow_tm_para.tm_trL);
	gpio_direction_input(chip_ctx->swi_gpio);
	ow_dbg_high();
	udelay(ow_tm_para.tm_tsamp);
	bit_data = gpio_get_value(chip_ctx->swi_gpio);
	ow_dbg_low();
	udelay(dly_read_end);
	gpio_direction_output(chip_ctx->swi_gpio, HIGH);
	return bit_data;
}

static void write_byte(struct fm1210 *chip_ctx, u8 val)
{
	u8 i;
	//unsigned long flags;

	mutex_lock(&chip_ctx->ow_access);
	/* writes byte, one bit at a time */
	for (i = 0; i < 8; i++)
		write_bit(chip_ctx, (val >> (7 - i)) & 0x01);
	udelay(ow_tm_para.tm_tbytegt);
	mutex_unlock(&chip_ctx->ow_access);
}

static u8 read_byte(struct fm1210 *chip_ctx)
{
	u8 i;
	u8 value = 0;
	//unsigned long flags;

	mutex_lock(&chip_ctx->ow_access);
	for (i = 0; i < 8; i++)
		if (read_bit(chip_ctx))
			value |= 0x01 << (7 - i);
	udelay(ow_tm_para.tm_tbytegt);
	mutex_unlock(&chip_ctx->ow_access);
	return value;
}

static void write_byte_hm(struct fm1210 *chip_ctx, u16 val)
{
	u8 i;
	//unsigned long flags;

	mutex_lock(&chip_ctx->ow_access);
	/* writes byte, one bit at a time */
	for (i = 0; i < 12; i++)
		write_bit(chip_ctx, (val >> (11 - i)) & 0x01);
	udelay(ow_tm_para.tm_tbytegt);
	mutex_unlock(&chip_ctx->ow_access);
}

static u16 read_byte_hm(struct fm1210 *chip_ctx)
{
	u8 i;
	u16 value = 0;
	//unsigned long flags;

	mutex_lock(&chip_ctx->ow_access);
	for (i = 0; i < 12; i++)
		if (read_bit(chip_ctx))
			value |= 0x01 << (11 - i);
	udelay(ow_tm_para.tm_tbytegt);
	mutex_unlock(&chip_ctx->ow_access);
	return value;
}

static u8 ow_se_reset(struct fm1210 *chip_ctx)
{
	gpio_direction_output(chip_ctx->swi_gpio, HIGH);
	//gpio_direction_output(OWSE_LPC, HIGH);
	gpio_set_value(chip_ctx->swi_gpio, LOW);
	//gpio_set_value(OWSE_LPC, LOW);
	udelay(ow_tm_para.tm_trstL);
	gpio_set_value(chip_ctx->swi_gpio, HIGH);
	//gpio_set_value(OWSE_LPC, HIGH);
	udelay(ow_tm_para.tm_trecRST);
	return 0;
}

struct ow_ops swi_ops = {
	.wbit = write_bit,
	.wbyte = write_byte,
	.rbit = read_bit,
	.rbyte = read_byte,
};

struct ow_ops_hm swi_ops_hm = {
	.wbit = write_bit,
	.wbyte = write_byte_hm,
	.rbit = read_bit,
	.rbyte = read_byte_hm,
};

/* default: SE */
static struct ow_ops *pswi_ops = &swi_ops;
static struct ow_ops_hm *pswi_ops_hm = &swi_ops_hm;

/*********************************************/
static unsigned short crc16_ccitt_false(unsigned char *puchMsg, unsigned int usDataLen)
{
	unsigned short wCRCin = 0xc6c6;
	unsigned short wCPoly = 0x1021;
	unsigned char wChar = 0;
	int i;

	while (usDataLen--) {
		wChar = *(puchMsg++);
		wCRCin ^= (wChar << 8);

		for (i = 0; i < 8; i++) {
			if (wCRCin & 0x8000)
				wCRCin = (wCRCin << 1) ^ wCPoly;
			else
				wCRCin = wCRCin << 1;
		}
	}
	return wCRCin;
}

u16 gen_hamming_code(u8 d)
{
	u8 a[4] = { 0xf0, 0x8e, 0x6d, 0x5b };
	u8 b, i, c, t;

	t = 0;
	for (i = 0; i < 4; i++) {
		b = a[i] & d;
		c = 0;
		while (b) {
			c++;
			b &= b - 1;
		}
		t <<= 1;
		if ((c & 1))
			t |= 1;
	}
	return d << 4 | t;
}

u8 check_hamming_code(u16 d)
{
	u8 a[4] = { 0xf0, 0x8e, 0x6d, 0x5b };
	u8 i, c, t;
	u16 b;

	t = 0;
	for (i = 0; i < 4; i++) {
		b = d & ((u16)a[i] << 4 | 1 << (3 - i));
		c = 0;
		while (b) {
			c++;
			b &= b - 1;
		}
		t <<= 1;
		if ((c & 1))
			t |= 1;
	}
	return t;
}

u8 rec_data_hamming_code(u16 d)
{
	u8 k = d >> 4;
	u8 e = check_hamming_code(d);

	switch (e) {
	case 3:
		hm_error_cnt++;
		return k ^ 1;
	case 5:
	case 6:
	case 7:
		hm_error_cnt++;
		return k ^ (1 << (e - 4));
	case 9:
	case 10:
	case 11:
	case 12:
		hm_error_cnt++;
		return k ^ (1 << (e - 5));
	case 0:
	case 1:
	case 2:
	case 4:
	case 8:
	default:
		return k;
	}
	return k;
}

static u8 ow_xfer(struct fm1210 *chip_ctx, u8 *sbuf, u16 slen, u8 *rbuf, u16 *rlen, u16 dlyms)
{
	u16 i;
	u16 crc_m, crc_s;
	u8 *wb = sbuf;
	u8 *rb = rbuf;

	if (!pswi_ops)
		return 0xAA;

	for (i = 0; i < slen; i++)
		pswi_ops->wbyte(chip_ctx, wb[i]);

	/* se cmd delay */
	mdelay(dlyms);

	*rlen = pswi_ops->rbyte(chip_ctx);
	if (*rlen < 1)
		return 0xA1;
	if (*rlen > 0xFD)
		return 0xA3;

	for (i = 0; i < *rlen + 2; i++)
		*(rb + i) = pswi_ops->rbyte(chip_ctx);

	crc_s = rb[*rlen] << 8 | rb[*rlen + 1];
	crc_m = crc16_ccitt_false(rb, *rlen);
	if (crc_m != crc_s)
		return 0xA2;

	return 0;
}

static u8 ow_transceive(struct fm1210 *chip_ctx, u8 *write_buf, u16 write_len, u8 *read_buf, u16 *read_len, u16 delay_ms)
{
	u8 buf[260];
	u16 rlen;
	u16 crc;
	u8 ret;
	u8 pos = 0;

	//unsigned long flags;

	//cmd len
	buf[pos++] = write_len;

	//cmd
	memcpy(buf + pos, write_buf, write_len);
	pos += write_len;

	//crc
	crc = crc16_ccitt_false(write_buf, write_len);
	buf[pos++] = crc >> 8;
	buf[pos++] = crc & 0xFF;

	*read_len = 0;
	//local_irq_save(flags);
	ret = ow_xfer(chip_ctx, buf, pos, buf, &rlen, delay_ms);
	//local_irq_restore(flags);
	mdelay(CMD_GAP_DELAY);
	if (ret)
		return ret;

	if (buf[0] != 0) {
		ret = (buf[0] | 0xB0);
		return ret;
	}

	rlen--; //cmd_len
	memcpy(read_buf, buf + 1, rlen);
	*read_len = rlen;
	return RESULT_SUCCESS;
}

static u8 ow_xfer_hm(struct fm1210 *chip_ctx, u16 *sbuf, u16 slen, u8 *rbuf, u16 *rlen, u16 dlyms)
{
	u16 i;
	u16 crc_s, crc_m;
	u16 *wb = sbuf;
	u8 *rb = rbuf;
	u16 recv_buf[260];
	u16 recv_len;

	if (!pswi_ops_hm)
		return 0xAA;
	for (i = 0; i < slen; i++)
		pswi_ops_hm->wbyte(chip_ctx, wb[i]);

	/* se cmd delay */
	mdelay(dlyms);

	recv_len = pswi_ops_hm->rbyte(chip_ctx);
	*rlen = rec_data_hamming_code(recv_len);
	if (recv_len == 0x0FFF)
		return 0xAF;

	if (*rlen < 1)
		return 0xA1;

	if (*rlen > 0xFD)
		return 0xA3;

	for (i = 0; i < *rlen + 2; i++)
		recv_buf[i] = pswi_ops_hm->rbyte(chip_ctx);

	for (i = 0; i < *rlen + 2; i++)
		*(rb + i) = rec_data_hamming_code(recv_buf[i]);

	crc_s = rb[*rlen] << 8 | rb[*rlen + 1];
	crc_m = crc16_ccitt_false(rb, *rlen);
	if (crc_m != crc_s) {
		pr_err("recv crc err\n");
		return 0xA2;
	}

	return 0;
}
static u8 ow_transceive_hm(struct fm1210 *chip_ctx, u8 *write_buf, u16 write_len, u8 *read_buf, u16 *read_len, u16 delay_ms)
{
	u16 buf[260];
	u8 rbuf[260];
	u16 rlen;
	u16 crc;
	u8 ret;
	u8 pos = 0;
	int i = 0;

	//unsigned long flags;

	//cmd len
	buf[pos++] = gen_hamming_code(write_len);

	//cmd
	for (i = 0; i < write_len; i++)
		buf[pos++] = gen_hamming_code(write_buf[i]);

	//crc
	crc = crc16_ccitt_false(write_buf, write_len);
	buf[pos++] = gen_hamming_code(crc >> 8);
	buf[pos++] = gen_hamming_code(crc & 0xFF);

	*read_len = 0;
	//local_irq_save(flags);
	ret = ow_xfer_hm(chip_ctx, buf, pos, rbuf, &rlen, delay_ms);
	//local_irq_restore(flags);
	mdelay(CMD_GAP_DELAY);
	if (ret)
		return ret;

	if (rbuf[0] != 0) {
		ret = (rbuf[0] | 0xB0);
		return ret;
	}

	rlen--; //cmd_len
	memcpy(read_buf, rbuf + 1, rlen);
	*read_len = rlen;
	return RESULT_SUCCESS;
}

/* apdu process */
u8 ow_apdu_xfer(struct fm1210 *chip_ctx, u8 *write_buf, u16 write_len, u8 *read_buf, u16 *read_len, u16 delay_ms)
{
	unsigned char ret = 0;
	u8 dev_sel[2] = { 0x10, 0x04 };

	ow_dbg_switch_gpio(3);
	for (j = 0; j < MAX_RETRY_COUNT; j++) {
		if (swi_protocol_hm_flag)
			ret = ow_transceive_hm(chip_ctx, write_buf, write_len, read_buf, read_len, delay_ms);
		else
			ret = ow_transceive(chip_ctx, write_buf, write_len, read_buf, read_len, delay_ms);

		if (!ret) {
			return RESULT_SUCCESS;
		} else if ((ret & 0xF0) == 0xA0) {
			ow_se_reset(chip_ctx);
			if (swi_protocol_hm_flag)
				ow_transceive_hm(chip_ctx, dev_sel, sizeof(dev_sel), read_buf, read_len, CMD_DEVICE_SELECT_DELAY);
			else
				ow_transceive(chip_ctx, dev_sel, sizeof(dev_sel), read_buf, read_len, CMD_DEVICE_SELECT_DELAY);
			pr_err("retry again! %d\n", j);
			mdelay(100);
		} else if ((ret & 0xF0) == 0xB0) {
			return ret;
		} else {
			return RESULT_NOKNOWN;
		}
	}
	ow_dbg_switch_gpio(5);
	return RESULT_FAIL;
}

static ssize_t ow_read_block(struct fm1210 *chip_ctx, u8 blockNum, char *outBuf)
{
	u8 cmdbuf[2] = { 0x30, 0x00 };
	u8 rbuf[128];
	u8 cmdlen = 2;
	u16 rlen;

	if (blockNum > 24 && blockNum != 0x38 && blockNum != 0x34 && (blockNum < 40 || blockNum > 48))
		return 1;
	cmdbuf[1] = blockNum;
	if (ow_apdu_xfer(chip_ctx, cmdbuf, cmdlen, rbuf, &rlen, CMD_READ_BLOCK_DELAY) != 0)
		return 2;
	memcpy(outBuf, (u8 *)rbuf, rlen);
	return 0;
}

static ssize_t ow_write_block(struct fm1210 *chip_ctx, u8 blockNum, char *inBuf)
{
	u8 cmdbuf[0x12] = { 0x31, 0x00 };
	u8 rbuf[128];
	u8 cmdlen = 0x12;
	u16 rlen;

	if (blockNum > 24 && blockNum != 0x34)
		return 1;
	cmdbuf[1] = blockNum;
	memcpy(cmdbuf + 2, inBuf, PAGE_BLOCK_SIZE);
	if (ow_apdu_xfer(chip_ctx, cmdbuf, cmdlen, rbuf, &rlen, CMD_WRITE_BLOCK_DELAY) != 0)
		return 2;
	return 0;
}

static ssize_t ow_lock_block_write(struct fm1210 *chip_ctx, u8 blockNum)
{
	u8 cmdbuf[2] = { 0x39, 0x00 };
	u8 rbuf[128];
	u8 cmdlen = 2;
	u16 rlen;

	if (blockNum > 24)
		return 1;
	cmdbuf[1] = blockNum;
	if (ow_apdu_xfer(chip_ctx, cmdbuf, cmdlen, rbuf, &rlen, CMD_LOCK_BLOCK_DELAY) != 0)
		return 2;
	return 0;
}

static ssize_t ow_read_block_config(struct fm1210 *chip_ctx, u8 blockNum, char *outBuf)
{
	u8 cmdbuf[2] = { 0x32, 0x00 };
	u8 rbuf[128];
	u8 cmdlen = 2;
	u16 rlen;

	if (blockNum > 24)
		return 1;
	cmdbuf[1] = blockNum;
	if (ow_apdu_xfer(chip_ctx, cmdbuf, cmdlen, rbuf, &rlen, CMD_READ_BLOCK_DELAY) != 0)
		return 2;
	memcpy(outBuf, (u8 *)rbuf, rlen);
	return 0;
}

static ssize_t ow_device_select(struct fm1210 *chip_ctx, u8 deviceAdr)
{
	u8 cmdbuf[0x02] = { 0x10, 0x04 };
	u8 rbuf[128];
	u8 cmdlen = 0x02;
	u16 rlen;

	cmdbuf[1] = deviceAdr;
	if (ow_apdu_xfer(chip_ctx, cmdbuf, cmdlen, rbuf, &rlen, CMD_DEVICE_SELECT_DELAY) != 0)
		return 1;
	return 0;
}

static ssize_t ow_ee_pump(struct fm1210 *chip_ctx, u8 flage)
{
	u8 cmdbuf[0x02] = { 0x15, 0x00 };
	u8 rbuf[128];
	u8 cmdlen = 0x02;
	u16 rlen;

	cmdbuf[1] = (flage == 0 ? 0x00 : 0x5A);
	if (ow_apdu_xfer(chip_ctx, cmdbuf, cmdlen, rbuf, &rlen, CMD_EE_PUMP_DELAY) != 0)
		return 1;
	return 0;
}

static ssize_t ow_read_cert(struct fm1210 *chip_ctx, u8 *outBuf)
{
	u8 cmdbuf[0x01] = { 0x40 };
	u8 rbuf[256];
	u8 cmdlen = 0x01;
	u16 rlen;

	if (ow_apdu_xfer(chip_ctx, cmdbuf, cmdlen, rbuf, &rlen, CMD_READ_CERT_DELAY) != 0)
		return 1;
	memcpy(outBuf, (u8 *)rbuf, rlen);
	return 0;
}
static ssize_t ow_read_cert_byblock(struct fm1210 *chip_ctx, u8 *outBuf)
{
	u8 rbuf[128];
	u8 ofs = 0;
	int i;

	pr_err("get_dev_cert_byblock in\n");
	for (i = 0; i < 9; i++) {
		if (ow_read_block(chip_ctx, 40+i, rbuf) != 0)
			return 1;
		memcpy(outBuf + ofs, rbuf, PAGE_BLOCK_SIZE);
		ofs += PAGE_BLOCK_SIZE;
	}
	return 0;
}

static ssize_t ow_sign_ecc(struct fm1210 *chip_ctx, u8 *inBuf, u8 inLen, u8 *outBuf)
{
	u8 cmdbuf[0x22] = { 0x41, 0x80 };
	u8 rbuf[128];
	u8 cmdlen = 0x22;
	u16 rlen;

	if (inLen != 0x20)
		return 1;
	memcpy(cmdbuf + 2, inBuf, inLen);
	if (ow_apdu_xfer(chip_ctx, cmdbuf, cmdlen, rbuf, &rlen, CMD_ECC_SIGN_DELAY) != 0)
		return 1;
	memcpy(outBuf, (u8 *)rbuf, rlen);
	return 0;
}

static u8 ow_page_test(struct fm1210 *chip_ctx)
{
	u8 rbuf[128];
	u8 ret = 0;
	int i;

	//file2 -->1Block
	//file1 -->4Block
	//file3 -->2Block
	for (i = 0; i < 7; i++) {
		ret = ow_read_block(chip_ctx, PAGE1_ADDR + i, rbuf);
		if (ret != 0)
			return 1;
	}
	return 0;
}

static u8 ow_allprocess(struct fm1210 *chip_ctx, u32 cnt)
{
	u8 ecc_sign_msg[0x20] = { 0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44 };
	u8 rbuf[146];

	while (cnt--) {
		//0.reset
		ow_se_reset(chip_ctx);
		if (ow_device_select(chip_ctx, CMD_OW_ADR) != 0) {
			chip_ctx->loop_error_cnt++;
			continue;
		}

		//1.read id
		if (ow_read_block(chip_ctx, ROMID_ADDR, rbuf) != 0) {
			chip_ctx->loop_error_cnt++;
			continue;
		}

		//2.read files,
		if (ow_page_test(chip_ctx) != 0) {
			chip_ctx->loop_error_cnt++;
			continue;
		}

		//3.read cert
		//if (ow_read_cert(rbuf) != 0) {
		if (ow_read_cert_byblock(chip_ctx, rbuf) != 0) {
			chip_ctx->loop_error_cnt++;
			continue;
		}

		//4.ecc sig
		if (ow_sign_ecc(chip_ctx, ecc_sign_msg, 0x20, rbuf) != 0) {
			chip_ctx->loop_error_cnt++;
			continue;
		}
	}
	return 0;
}

static u8 ow_readpage(struct fm1210 *chip_ctx, u32 cnt)
{
	int i;

	for (i = 0; i < cnt; i++)
		if (ow_page_test(chip_ctx) != 0)
			return 1;
	return 0;
}

/********************************************************/
static ssize_t ow_reset(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fm1210 *chip_ctx;
	
	chip_ctx = dev_get_drvdata(dev);
	ow_se_reset(chip_ctx);
	if (ow_device_select(chip_ctx, CMD_OW_ADR) != 0)
		return sprintf(buf, "reset se fail\n");
	return sprintf(buf, "reset se success\n");
}

static ssize_t read_romid(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 rbuf[146];
	struct fm1210 *chip_ctx;
	
	chip_ctx = dev_get_drvdata(dev);
	if (ow_read_block(chip_ctx, ROMID_ADDR, rbuf) != 0)
		return sprintf(buf, "get romid err\n");
	hex_to_str(buf, rbuf, READ_UID_SIZE);
	sprintf(buf + READ_UID_OFS, "\n");
	return READ_UID_RET;
}

static ssize_t init_se(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fm1210 *chip_ctx;
	
	chip_ctx = dev_get_drvdata(dev);
	gpio_direction_output(chip_ctx->dbg_gpio, HIGH);

	//init ow time
	ow_set_tm((struct ow_tm_data *)ow_tm[swi_protocol_para_index]);

	ow_se_reset(chip_ctx);
	hm_error_cnt = 0;

	mdelay(10);
	if (ow_device_select(chip_ctx, CMD_OW_ADR) != 0)
		return sprintf(buf, "init se err\n");

	return sprintf(buf, "init se ok\n");
}

ssize_t get_dev_cert(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 rbuf[146];
	struct fm1210 *chip_ctx;
	
	chip_ctx = dev_get_drvdata(dev);
	if (ow_read_cert(chip_ctx, rbuf) != 0)
		return sprintf(buf, "get cert err\n");
	hex_to_str(buf, rbuf, DEV_CERT_SIZE);
	sprintf(buf + DEV_CERT_OFS, "\n");
	return READ_CERT_RET;
}

ssize_t get_dev_cert_byblock(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 rbuf[128];
	u8 tbuf[146];
	u8 ofs = 0;
	int i;
	struct fm1210 *chip_ctx;
	
	chip_ctx = dev_get_drvdata(dev);
	for (i = 0; i < 9; i++) {
		if (ow_read_block(chip_ctx, 40+i, rbuf) != 0)
			return sprintf(buf, "get cert err\n");
		memcpy(tbuf + ofs, rbuf, PAGE_BLOCK_SIZE);
		ofs += PAGE_BLOCK_SIZE;
	}

	hex_to_str(buf, tbuf, DEV_CERT_SIZE);
	sprintf(buf + DEV_CERT_OFS, "\n");
	return READ_CERT_RET;
}

ssize_t ecc_signature(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	u8 rbuf[66];
	u8 msg[32] = { 0 };
	u16 msglen;
	u8 ret;
	struct fm1210 *chip_ctx;
	
	chip_ctx = dev_get_drvdata(dev);
	if (count > ECC_HASH_MAX_SIZE)
		return count;
	msglen = (count - 1) / 2;
	str_to_hex(msg, buf, msglen);

	ret = ow_sign_ecc(chip_ctx, msg, msglen, rbuf);
	if (ret == 0)
		memcpy(ecc_sig_res, rbuf, 0x40);
	return count;
}

ssize_t get_sig_res(struct device *dev, struct device_attribute *attr, char *buf)
{
	hex_to_str(buf, ecc_sig_res, ECC_SIG_SIZE);
	sprintf(buf + ECC_SIG_OFS, "\n");
	memset(ecc_sig_res, 0, sizeof(ecc_sig_res));
	return ECC_SIG_RET;
}

ssize_t read_page2(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* page2 : 4-block */
	u8 rbuf[128];
	u8 tbuf[256];
	u8 ofs = 0;
	int i;
	struct fm1210 *chip_ctx;
	
	chip_ctx = dev_get_drvdata(dev);
	for (i = 0; i < PAGE2_BLOCK; i++) {
		if (ow_read_block(chip_ctx, PAGE2_ADDR + i, rbuf) != 0)
			return sprintf(buf, "read page2 err\n");
		memcpy(tbuf + ofs, rbuf, PAGE_BLOCK_SIZE);
		ofs += PAGE_BLOCK_SIZE;
	}

	hex_to_str(buf, tbuf, PAGE2_SIZE);
	sprintf(buf + PAGE2_OFS, "\n");
	return PAGE2_RET;
}


ssize_t write_page2(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	/* page2 : 4-block */
	u8 tbuf[256];
	u8 msg[PAGE2_BLOCK * PAGE_BLOCK_SIZE];
	u16 msglen;
	u8 ofs = 0;
	int i;
	struct fm1210 *chip_ctx;
	
	chip_ctx = dev_get_drvdata(dev);
	if (count > PAGE2_RET)
		return RESULT_ERROR;

	msglen = (count - 1) / 2;
	str_to_hex(msg, buf, msglen);
	ofs = PAGE2_BLOCK * PAGE_BLOCK_SIZE - msglen;
	memcpy(tbuf, msg, msglen);
	if (ofs)
		memset(tbuf + msglen, 0, ofs);
	ow_ee_pump(chip_ctx, EE_PUMP_ACTIVE);
	ofs = 0;
	for (i = 0; i < PAGE2_BLOCK; i++) {
		if (ow_write_block(chip_ctx, PAGE2_ADDR + i, tbuf + ofs) != 0)
			return RESULT_ERROR;
		ofs += PAGE_BLOCK_SIZE;
	}
	ow_ee_pump(chip_ctx, EE_PUMP_CLOSE);

	return count;
}

ssize_t lock_page2(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int i;
	u8 ret = 0;
	struct fm1210 *chip_ctx;
	
	chip_ctx = dev_get_drvdata(dev);
	if (!memcmp(buf, "lock page2", LOCK_PAGE_RET)) {
		ow_ee_pump(chip_ctx, EE_PUMP_ACTIVE);
		for (i = 0; i < PAGE2_BLOCK; i++)
			ret |= ow_lock_block_write(chip_ctx, PAGE2_ADDR + i);
		ow_ee_pump(chip_ctx, EE_PUMP_CLOSE);
		if (ret != 0)
			return RESULT_ERROR;
	} else
		return RESULT_ERROR;

	return count;
}


ssize_t page2_sta(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* page2 : 4-block */
	u8 rbuf[128];
	struct file_status page2status;
	int i;
	int lockblock = 0;
	int clearblock = 0;
	struct fm1210 *chip_ctx;
	
	chip_ctx = dev_get_drvdata(dev);
	for (i = 0; i < PAGE2_BLOCK; i++) {
		if (ow_read_block_config(chip_ctx, PAGE2_ADDR + i, rbuf) != 0)
			return sprintf(buf, "read page2 config err\n");
		memcpy((u8 *)&page2status, rbuf, sizeof(page2status));
		if ((page2status.write_access != ACCESS_ALLOWED) && (page2status.read_access == ACCESS_ALLOWED) && (page2status.enc_type == ENC_CLEARTEXT))
			lockblock++;
		else if ((page2status.write_access == ACCESS_ALLOWED) && (page2status.read_access == ACCESS_ALLOWED) && (page2status.enc_type == ENC_CLEARTEXT))
			clearblock++;
		else
			return sprintf(buf, "page2 config err. wrong status.\n");

	}
	if (lockblock == PAGE2_BLOCK)
		return sprintf(buf, "page2 is locked\n");
	else if (clearblock == PAGE2_BLOCK)
		return sprintf(buf, "page2 is unlocked\n");
	else
		return sprintf(buf, "page2 config err. not fully locked. %d blocks are locked, %d blocks are unlocked.\n", lockblock, clearblock);
}

ssize_t read_page1(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* page1 : 1-block */
	u8 rbuf[128];
	u8 tbuf[256];
	u8 ofs = 0;
	int i;
	struct fm1210 *chip_ctx;
	
	chip_ctx = dev_get_drvdata(dev);
	for (i = 0; i < PAGE1_BLOCK; i++) {
		if (ow_read_block(chip_ctx, PAGE1_ADDR + i, rbuf) != 0)
			return sprintf(buf, "read page1 err\n");
		memcpy(tbuf + ofs, rbuf, PAGE_BLOCK_SIZE);
		ofs += PAGE_BLOCK_SIZE;
	}

	hex_to_str(buf, tbuf, PAGE1_SIZE);
	sprintf(buf + PAGE1_OFS, "\n");
	return PAGE1_RET;
}

ssize_t read_page3(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* page3 : 24-byte, 2-block */
	u8 rbuf[128];
	u8 tbuf[256];
	u8 ofs = 0;
	int i;
	struct fm1210 *chip_ctx;
	
	chip_ctx = dev_get_drvdata(dev);
	for (i = 0; i < 2; i++) {
		if (ow_read_block(chip_ctx, PAGE3_ADDR + i, rbuf) != 0)
			return sprintf(buf, "read page3 err\n");
		memcpy(tbuf + ofs, rbuf, PAGE_BLOCK_SIZE);
		ofs += PAGE_BLOCK_SIZE;
	}


	hex_to_str(buf, tbuf, PAGE3_SIZE);
	sprintf(buf + PAGE3_OFS, "\n");
	return PAGE3_RET;
}

//shengchanshijian tbuf 05 02849 11 5 9 01 1   50714
ssize_t read_battery_mac(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* page1 : 1-block */
	u8 rbuf[128];
	u8 tbuf[256];
	u8 ofs = 0;
	int i;
	struct fm1210 *chip_ctx;
	
	chip_ctx = dev_get_drvdata(dev);
	for (i = 0; i < PAGE1_BLOCK; i++) {
		if (ow_read_block(chip_ctx, PAGE1_ADDR + i, rbuf) != 0)
			return sprintf(buf, "read page1 err\n");
		memcpy(tbuf + ofs, rbuf, PAGE_BLOCK_SIZE);
		ofs += PAGE_BLOCK_SIZE;
	}

	sprintf(buf,"%d-%02d-%d%d\n",(tbuf[9]- 0x30),(tbuf[10]- 0x30),(tbuf[11]- 0x30),(tbuf[12] - 0x30));
	pr_err("%x,%x,%x%x\n",tbuf[9],tbuf[10],tbuf[11],tbuf[12]);
	return PAGE1_RET;
}

ssize_t read_active_date(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* PAGE5 : 12-byte, 1-block */
	u8 rbuf[128];
	u8 tbuf[256];
	u8 ofs = 0;
	int i;
	struct fm1210 *chip_ctx;
	
	chip_ctx = dev_get_drvdata(dev);
	for (i = 0; i < PAGE6_BLOCK; i++) {
		if (ow_read_block(chip_ctx, PAGE6_ADDR + i, rbuf) != 0)
			return sprintf(buf, "read PAGE5 err\n");
		memcpy(tbuf + ofs, rbuf, PAGE_BLOCK_SIZE);
		ofs += PAGE_BLOCK_SIZE;
	}

	pr_err("%x,%x,%x,%x\n",tbuf[0],tbuf[1],tbuf[2],tbuf[3]);
	sprintf(buf, "%c-%c-%c%c\n",tbuf[0],tbuf[1],tbuf[2],tbuf[3]);
	return PAGE6_RET;
}

ssize_t write_active_date(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	/* PAGE6 : 4-block */
	u8 tbuf[256];
	u8 msg[PAGE6_BLOCK * PAGE_BLOCK_SIZE];
	u16 msglen;
	u8 ofs = 0;
	int i;
	struct fm1210 *chip_ctx;
	
	chip_ctx = dev_get_drvdata(dev);
	if (count > PAGE6_RET)
		return RESULT_ERROR;

	msglen = (count - 1) / 2;
	str_to_hex(msg, buf, msglen);
	ofs = PAGE6_BLOCK * PAGE_BLOCK_SIZE - msglen;
	memcpy(tbuf, msg, msglen);
	if (ofs)
		memset(tbuf + msglen, 0, ofs);
	ow_ee_pump(chip_ctx, EE_PUMP_ACTIVE);
	ofs = 0;
	for (i = 0; i < PAGE6_BLOCK; i++) {
		if (ow_write_block(chip_ctx, PAGE6_ADDR + i, tbuf + ofs) != 0)
			return RESULT_ERROR;
		ofs += PAGE_BLOCK_SIZE;
	}
	ow_ee_pump(chip_ctx, EE_PUMP_CLOSE);

	return count;
}

ssize_t read_battery_health(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* PAGE7 : 12-byte, 1-block */
	u8 rbuf[128];
	u8 tbuf[256];
	u8 ofs = 0;
	int i;
	struct fm1210 *chip_ctx;
	
	chip_ctx = dev_get_drvdata(dev);
	for (i = 0; i < 2; i++) {
		if (ow_read_block(chip_ctx, PAGE7_ADDR + i, rbuf) != 0)
			return sprintf(buf, "read PAGE7 err\n");
		memcpy(tbuf + ofs, rbuf, PAGE_BLOCK_SIZE);
		ofs += PAGE_BLOCK_SIZE;
	}

	pr_err("%x,%x,%x,%x\n",tbuf[0],tbuf[1],tbuf[2],tbuf[3]);
	sprintf(buf, "%s\n",tbuf);
	return PAGE7_RET;
}

ssize_t write_battery_health(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	/* PAGE7 : 4-block */
	u8 tbuf[256];
	u8 msg[PAGE7_BLOCK * PAGE_BLOCK_SIZE];
	u16 msglen;
	u8 ofs = 0;
	int i;
	struct fm1210 *chip_ctx;
	
	chip_ctx = dev_get_drvdata(dev);
	if (count > PAGE7_RET)
		return RESULT_ERROR;

	msglen = (count - 1) / 2;
	str_to_hex(msg, buf, msglen);
	ofs = PAGE7_BLOCK * PAGE_BLOCK_SIZE - msglen;
	memcpy(tbuf, msg, msglen);
	if (ofs)
		memset(tbuf + msglen, 0, ofs);
	ow_ee_pump(chip_ctx, EE_PUMP_ACTIVE);
	ofs = 0;
	for (i = 0; i < PAGE7_BLOCK; i++) {
		if (ow_write_block(chip_ctx, PAGE7_ADDR + i, tbuf + ofs) != 0)
			return RESULT_ERROR;
		ofs += PAGE_BLOCK_SIZE;
	}
	ow_ee_pump(chip_ctx, EE_PUMP_CLOSE);

	return count;
}

ssize_t read_battery_cycle(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* PAGE8 : 12-byte, 1-block */
	u8 rbuf[128];
	u8 tbuf[256];
	u8 ofs = 0;
	int i;
	struct fm1210 *chip_ctx;
	
	chip_ctx = dev_get_drvdata(dev);
	for (i = 0; i < 2; i++) {
		if (ow_read_block(chip_ctx, PAGE8_ADDR + i, rbuf) != 0)
			return sprintf(buf, "read PAGE8 err\n");
		memcpy(tbuf + ofs, rbuf, PAGE_BLOCK_SIZE);
		ofs += PAGE_BLOCK_SIZE;
	}

	pr_err("%x,%x,%x,%x\n",tbuf[0],tbuf[1],tbuf[2],tbuf[3]);
	sprintf(buf, "%s\n",tbuf);
	return PAGE8_RET;
}
ssize_t write_battery_cycle(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	/* PAGE8 : 4-block */
	u8 tbuf[256];
	u8 msg[PAGE8_BLOCK * PAGE_BLOCK_SIZE];
	u16 msglen;
	u8 ofs = 0;
	int i;
	struct fm1210 *chip_ctx;
	
	chip_ctx = dev_get_drvdata(dev);
	if (count > PAGE8_RET)
		return RESULT_ERROR;

	msglen = (count - 1) / 2;
	str_to_hex(msg, buf, msglen);
	ofs = PAGE8_BLOCK * PAGE_BLOCK_SIZE - msglen;
	memcpy(tbuf, msg, msglen);
	if (ofs)
		memset(tbuf + msglen, 0, ofs);
	ow_ee_pump(chip_ctx, EE_PUMP_ACTIVE);
	ofs = 0;
	for (i = 0; i < PAGE8_BLOCK; i++) {
		if (ow_write_block(chip_ctx, PAGE8_ADDR + i, tbuf + ofs) != 0)
			return RESULT_ERROR;
		ofs += PAGE_BLOCK_SIZE;
	}
	ow_ee_pump(chip_ctx, EE_PUMP_CLOSE);

	return count;
}

ssize_t read_page_all(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* 13-block 208Byte */
	u8 rbuf[128];
	u8 tbuf[256];
	u8 ofs = 0;
	int i;
	struct fm1210 *chip_ctx;
	
	chip_ctx = dev_get_drvdata(dev);
	for (i = 0; i < 13; i++) {
		if (ow_read_block(chip_ctx, PAGE1_ADDR + i, rbuf) != 0)
			return sprintf(buf, "read pageAll err\n");
		memcpy(tbuf + ofs, rbuf, PAGE_BLOCK_SIZE);
		ofs += PAGE_BLOCK_SIZE;
	}

	hex_to_str(buf, tbuf, PAGE_ALL_SIZE);
	sprintf(buf + PAGE_ALL_OFS, "\n");
	return PAGE_ALL_RET;
}

ssize_t set_para(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	u8 msg[32] = { 0 };
	u16 msglen;
	u8 hm = 0;

	//u8 ret;
	pr_err("fm1210 set_para in\n");
	if (count > PARA_MAX_RET)
		return RESULT_ERROR;
	msglen = (count - 1) / 2;
	str_to_hex(msg, buf, msglen);
	swi_protocol_para_index = msg[0];
	hm = msg[1];

	ow_set_tm((struct ow_tm_data *)ow_tm[swi_protocol_para_index]);
	swi_protocol_hm_flag = hm;

	//ow_se_reset();
	pr_err("fm1210 set_para out count\n");
	return count;
}

ssize_t get_para(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fm1210 *chip_ctx;

	chip_ctx = dev_get_drvdata(dev);
	u8 para[8] = { 0 };
	u32 strLen = 0;
	pr_err("fm1210 get_para in\n");
	para[0] = swi_protocol_para_index;
	para[1] = swi_protocol_hm_flag;
	para[2] = hm_error_cnt >> 24 & 0xFF;
	para[3] = hm_error_cnt >> 16 & 0xFF;
	para[4] = hm_error_cnt >> 8 & 0xFF;
	para[5] = hm_error_cnt & 0xFF;
	para[6] = j >> 8;
	para[7] = j & 0xFF;
	hex_to_str(buf, para, PARA_LENGTH);
	strLen = sprintf(buf, "***HOST protocol para***\n");
	strLen += sprintf(buf + strLen, "protocol_para_index:%d,\n", swi_protocol_para_index);
	strLen += sprintf(buf + strLen, "loop_cnt:%d\nloop_error_cnt:%d,\nhm:%d,\nhm_error_cnt:%d,\nj:%d,\n", chip_ctx->loop_cnt, chip_ctx->loop_error_cnt, swi_protocol_hm_flag, hm_error_cnt, j);
	strLen += sprintf(buf + strLen, "tm_trstL:%d,\ntm_trecRST:%d,\ntm_trec:%d,\n", ow_tm_para.tm_trstL, ow_tm_para.tm_trecRST, ow_tm_para.tm_trec);
	strLen += sprintf(buf + strLen, "tm_tw1L:%d,\ntm_tw0L:%d,\n", ow_tm_para.tm_tw1L, ow_tm_para.tm_tw0L);
	strLen += sprintf(buf + strLen, "tm_trL:%d,\ntm_trgt:%d,\ntm_tpdL:%d,\n", ow_tm_para.tm_trL, ow_tm_para.tm_trgt, ow_tm_para.tm_tpdL);
	strLen += sprintf(buf + strLen, "tm_tbytegt:%d,\ntm_tcmdgt:%d,\ntm_tsamp:%d,\n",  ow_tm_para.tm_tbytegt, ow_tm_para.tm_tcmdgt, ow_tm_para.tm_tsamp);
	return strLen;
}

u8 ow_set_time(u8 *para)
{
	u8 ret = 0;
	u8 time_type = *para;
	u16 time_val = be16p_to_cpu(para + 1);

	//pr_err("%02x,%04x\n",time_type,time_val);
	switch (time_type) {
	case 0:
		//ow_set_tm((ow_tm_data *)ow_tm[0]);
		break;
	case 1:
		ow_tm_para.tm_trstL = time_val;
		break;
	case 2:
		ow_tm_para.tm_trecRST = time_val;
		break;
	case 3:
		ow_tm_para.tm_trec = time_val;
		break;
	case 4:
		ow_tm_para.tm_tw1L = time_val;
		break;
	case 5:
		ow_tm_para.tm_tw0L = time_val;
		break;
	case 6:
		ow_tm_para.tm_trL = time_val;
		break;
	case 7:
		ow_tm_para.tm_trgt = time_val;
		break;
	case 8:
		ow_tm_para.tm_tpdL = time_val;
		break;
	case 9:
		ow_tm_para.tm_tbytegt = time_val;
		break;
	case 10:
		ow_tm_para.tm_tcmdgt = time_val;
		break;
	case 11:
		ow_tm_para.tm_tsamp = time_val;
		break;
	default:
		break;
	}
	return ret;
}

ssize_t set_time(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	u8 msg[32] = { 0 };
	u16 msglen;

	if (count > TIME_MAX_SIZE)
		return RESULT_ERROR;
	msglen = (count - 1) / 2;
	str_to_hex(msg, buf, msglen);

	ow_set_time(msg);

	return count;
}

ssize_t get_time(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 para[32] = { 0 };

	para[0] = ow_tm_para.tm_trstL >> 8;
	para[1] = ow_tm_para.tm_trstL;
	para[2] = ow_tm_para.tm_trecRST >> 8;
	para[3] = ow_tm_para.tm_trecRST;
	para[4] = ow_tm_para.tm_trec >> 8;
	para[5] = ow_tm_para.tm_trec;
	para[6] = ow_tm_para.tm_tw1L >> 8;
	para[7] = ow_tm_para.tm_tw1L;
	para[8] = ow_tm_para.tm_tw0L >> 8;
	para[9] = ow_tm_para.tm_tw0L;
	para[10] = ow_tm_para.tm_trL >> 8;
	para[11] = ow_tm_para.tm_trL;
	para[12] = ow_tm_para.tm_trgt >> 8;
	para[13] = ow_tm_para.tm_trgt;
	para[14] = ow_tm_para.tm_tpdL >> 8;
	para[15] = ow_tm_para.tm_tpdL;
	para[16] = ow_tm_para.tm_tbytegt >> 8;
	para[17] = ow_tm_para.tm_tbytegt;
	para[18] = ow_tm_para.tm_tcmdgt >> 8;
	para[19] = ow_tm_para.tm_tcmdgt;
	para[20] = ow_tm_para.tm_tsamp >> 8;
	para[21] = ow_tm_para.tm_tsamp;
	hex_to_str(buf, para, TIME_LENGTH);
	sprintf(buf + TIME_OFS, "\n");
	return TIME_RET;
}

ssize_t owtest_allprocess(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	u8 msg[32] = { 0 };
	u16 msglen;
	static u32 loop;
	struct fm1210 *chip_ctx;
	
	chip_ctx = dev_get_drvdata(dev);
	if (count < LOOP_PARA_SIZE)
		return RESULT_ERROR;
	msglen = (count - 1) / 2;
	str_to_hex(msg, buf, msglen);
	loop = msg[0] << 24 | msg[1] << 16 | msg[2] << 8 | msg[3];

	hm_error_cnt = 0;
	chip_ctx->loop_cnt = loop;
	chip_ctx->loop_error_cnt = 0;
	ow_allprocess(chip_ctx, loop);

	return count;
}

ssize_t owtest_readpage(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	u8 msg[32] = { 0 };
	u16 msglen;
	static u32 loop;
	struct fm1210 *chip_ctx;
	
	chip_ctx = dev_get_drvdata(dev);
	if (count < LOOP_PARA_SIZE)
		return RESULT_ERROR;
	msglen = (count - 1) / 2;
	str_to_hex(msg, buf, msglen);
	loop = msg[0] << 24 | msg[1] << 16 | msg[2] << 8 | msg[3];
	ow_readpage(chip_ctx, loop);

	return count;
}

ssize_t owtest_verifytest(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i;
	u8 ofs = 0;
	int ret;
	char cert[146];
	u8 rbuf[128];
	u8 hash[32];
	unsigned char trnd[8];
	const u8 vendor_pbk[64] = {	0x92, 0x57, 0x54, 0xF8, 0x07, 0xE2, 0x10, 0x86,
					0xE6, 0xAB, 0xE1, 0x97, 0xCD, 0x16, 0xDB, 0x88,
					0xC5, 0x84, 0xF8, 0x9F, 0xC9, 0xA3, 0x71, 0x1F,
					0x7F, 0xAB, 0x94, 0xF8, 0x46, 0x49, 0xD4, 0x4C,
					0x23, 0xA8, 0xFF, 0x94, 0x87, 0x70, 0x52, 0xA0,
					0x66, 0xDA, 0xF8, 0x15, 0xB7, 0x4B, 0x8A, 0xCE,
					0xE3, 0xA1, 0xFE, 0x1E, 0xB6, 0x25, 0x82, 0x3B,
					0xAD, 0xDF, 0x7D, 0x0F, 0xBC, 0x3F, 0xCC, 0xE3,};
	u8 *CombTable_QBuf = kmalloc(1920, GFP_KERNEL);
	//struct timespec start_time,end_time;
	//ktime_t start_ktime, end_ktime;
	//s64 timegap;
	struct fm1210 *chip_ctx;
	if (CombTable_QBuf == NULL)
                return -1;

	chip_ctx = dev_get_drvdata(dev);
	//ktime_get_ts(&start_time);
	pr_err("fm1210 verify start\n");

	//0.set protocal para + rst + device select
	ow_set_tm((struct ow_tm_data *)ow_tm[2]);
	swi_protocol_hm_flag = 0;
	ow_se_reset(chip_ctx);
	mdelay(10);
	if (ow_device_select(chip_ctx, CMD_OW_ADR) != 0) {
		kvfree(CombTable_QBuf);
		return sprintf(buf, "init se err\n");
	}

	//1.verify cert
	//1.1 vernder public key init
	fm1210_ecdsa_precombpoint(CombTable_QBuf, 1920, vendor_pbk, 64);
	//ds_dbg("ecdsa init success\n");
	//1.2 read cert
	for (i = 0; i < 9; i++) {
		if (ow_read_block(chip_ctx, 40+i, rbuf) != 0) {
			kvfree(CombTable_QBuf);
			return sprintf(buf, "get cert err\n");
		}
		memcpy(cert + ofs, rbuf, PAGE_BLOCK_SIZE);
		ofs += PAGE_BLOCK_SIZE;
	}

	//1.2 hash(cert head)
	fm1210_ecdsa_sha256(cert, 80, hash);
	//ds_dbg("AP CERT Hash-SHA256:\n");
	//dump_data(hash, 32);

	//1.3 verify cert
	ret = fm1210_ecdsa_verify(CombTable_QBuf, 1920, vendor_pbk, 64, hash, 32, cert + 80, 64);
	if (!ret) {
		ds_dbg(" verify cert success\n");
	} else {
		kvfree(CombTable_QBuf);
		return sprintf(buf, "AP verify cert fail\n");
	}
	//ds_dbg("verify\n");

	//2.verify SE public key
	//2.1 SE public key init
	fm1210_ecdsa_precombpoint(CombTable_QBuf, 1920, cert + 16, 64);
	get_random_bytes(trnd, 8);
	//ds_dbg("AP Terminal TRAND:\n");
	//dump_data(trnd, 8);
	//2.2  hash(trnd)
	fm1210_ecdsa_sha256(trnd, 8, hash);
	//2.3  SE calculate ECC signature
	ret = ow_sign_ecc(chip_ctx, hash, 32, rbuf);
	if (ret) {
		kvfree(CombTable_QBuf);
		return sprintf(buf, "AP verify fail. Private key signature error.\n");
	}
	//ds_dbg("OWSE ECC signature:\n");
	//dump_data(rbuf, 64);

	//1.3 verify cert
	ret = fm1210_ecdsa_verify(CombTable_QBuf, 1920, cert + 16, 64, hash, 32, rbuf, 64);
	//ktime_get_ts(&end_time);
	if (!ret)
		ds_dbg("AP verify signature success\n");
	else
		sprintf( buf,"AP verify signature fail. SE pro\n");
	//start_ktime = timespec_to_ktime(start_time);
	//end_ktime = timespec_to_ktime(end_time);

	//timegap=ktime_to_ms(ktime_sub(end_ktime,start_ktime));
	//pr_err("start:%llu,end:%llu,gap:%llu ",ktime_to_ns(start_ktime),ktime_to_ns(end_ktime),timegap);
	pr_err("fm1210 verify success\n");
	kvfree(CombTable_QBuf);

	return sprintf(buf, "verify success\n");
}

static u8 toggle_gpio_test(struct fm1210 *chip_ctx, u32 cnt)
{
	int i;

	gpio_direction_output(chip_ctx->swi_gpio, HIGH);
	for (i = 0; i < cnt; i++) {
		gpio_set_value(chip_ctx->swi_gpio, LOW);
		gpio_set_value(chip_ctx->swi_gpio, HIGH);
	}
	return 0;
}

ssize_t ow_toggle_gpio(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	u8 msg[32] = { 0 };
	u16 msglen = 0;
	static u32 loop;
	struct fm1210 *chip_ctx;
	
	chip_ctx = dev_get_drvdata(dev);	
	if (count < TOGLE_PARA_SIZE)
		return RESULT_ERROR;
	msglen = (count - 1) / 2;
	str_to_hex(msg, buf, msglen);
	loop = msg[0] << 24 | msg[1] << 16 | msg[2] << 8 | msg[3];

	mutex_lock(&chip_ctx->ow_access);
	toggle_gpio_test(chip_ctx, loop);
	mutex_unlock(&chip_ctx->ow_access);

	return count;
}

static DEVICE_ATTR(ow_init, S_IRUGO, init_se, NULL);
static DEVICE_ATTR(ow_reset, S_IRUGO, ow_reset, NULL);
static DEVICE_ATTR(ow_romid, S_IRUGO, read_romid, NULL);
static DEVICE_ATTR(ow_devcert, S_IRUGO, get_dev_cert, NULL);
static DEVICE_ATTR(ow_devcert_byblock, S_IRUGO, get_dev_cert_byblock, NULL);
static DEVICE_ATTR(ow_eccsig, S_IRUGO | S_IWUSR | S_IWGRP, get_sig_res, ecc_signature);
static DEVICE_ATTR(ow_page1, S_IRUGO, read_page1, NULL);
static DEVICE_ATTR(ow_page2, S_IRUGO | S_IWUSR | S_IWGRP, read_page2, write_page2);
static DEVICE_ATTR(ow_battery_mac, S_IRUGO, read_battery_mac, NULL);
static DEVICE_ATTR(ow_active_date, S_IRUGO | S_IWUSR | S_IWGRP, read_active_date, write_active_date);
static DEVICE_ATTR(ow_battery_health, S_IRUGO | S_IWUSR | S_IWGRP, read_battery_health, write_battery_health);
static DEVICE_ATTR(ow_battery_cycle, S_IRUGO | S_IWUSR | S_IWGRP, read_battery_cycle, write_battery_cycle);
static DEVICE_ATTR(ow_page3, S_IRUGO, read_page3, NULL);
static DEVICE_ATTR(ow_page_all, S_IRUGO, read_page_all, NULL);
static DEVICE_ATTR(ow_para, S_IRUGO | S_IWUSR | S_IWGRP, get_para, set_para);
static DEVICE_ATTR(owtest_allprocess, S_IWUSR | S_IWGRP, NULL, owtest_allprocess);
static DEVICE_ATTR(owtest_readpage, S_IWUSR | S_IWGRP, NULL, owtest_readpage);
static DEVICE_ATTR(ow_toggle, S_IWUSR | S_IWGRP, NULL, ow_toggle_gpio);
static DEVICE_ATTR(ow_lockpage2, S_IRUGO | S_IWUSR | S_IWGRP, page2_sta, lock_page2);
static DEVICE_ATTR(ow_verify, S_IRUGO, owtest_verifytest, NULL);
static struct attribute *ds_attributes[] = {
	&dev_attr_ow_init.attr,
	&dev_attr_ow_reset.attr,
	&dev_attr_ow_romid.attr,
	&dev_attr_ow_devcert.attr,
	&dev_attr_ow_devcert_byblock.attr,
	&dev_attr_ow_eccsig.attr,
	&dev_attr_ow_page1.attr,
	&dev_attr_ow_page2.attr,
	&dev_attr_ow_battery_mac.attr,
	&dev_attr_ow_active_date.attr,
	&dev_attr_ow_battery_health.attr,
	&dev_attr_ow_battery_cycle.attr,
	&dev_attr_ow_page3.attr,
	&dev_attr_ow_page_all.attr,
	&dev_attr_ow_para.attr,
	&dev_attr_owtest_allprocess.attr,
	&dev_attr_owtest_readpage.attr,
	&dev_attr_ow_toggle.attr,
	&dev_attr_ow_lockpage2.attr,
	&dev_attr_ow_verify.attr,
	NULL,
};
static const struct attribute_group ds_attr_group = {
	.attrs	= ds_attributes,
};

#if IS_ENABLED(CONFIG_TRAN_TRANLOG)
extern int register_cloudengine_notifier(const char* id, const char* ver, struct notifier_block *nb);
extern int unregister_cloudengine_notifier(const char* id, struct notifier_block *nb);
extern void cloudengine_feed_back(const char* id, int state);
extern char* cloudengine_getConfig(const char* id);
extern char* cloudengine_getStringByContent(char* content, const char* key);
extern int cloudengine_getIntByContent(char* content, const char* key);
extern char* cloudengine_getStringByID(const char* id, const char* key);
extern int cloudengine_getIntByID(const char* id, const char* key);

enum {
	NO_FEEDBACK,
	FEEDBACK_ERR,
	FEEDBACK_GOOD,
};

static int bat_authent_cloud_enable(struct fm1210 *chip_ctx)
{
	if (IS_ERR_OR_NULL(chip_ctx))
		return 0;

	if (chip_ctx->bat_authent_cloud_feedback == FEEDBACK_GOOD)
		return 1;
	return 0;
}

static int bat_authent_notifier_call(struct notifier_block *nb, unsigned long action, void *data)
{
	struct fm1210 *chip_ctx = container_of(nb, struct fm1210, bat_authent_notifier_block);
	struct cloud_engine_config *cec;
	char *id;
	char *config;
	int v = 0;

	if (NULL == data)
		return -1;

	cec = (struct cloud_engine_config *)data;
	id = cec->id;
	config = cec->config;
	if (strcmp(id, chip_ctx->regID)) {
		pr_err("bat authent cloudengine id mismatch\n");
		return 0;
	}

	cloudengine_feed_back(chip_ctx->regID, 1);

	v = cloudengine_getIntByID(chip_ctx->regID, "bat_authent_ctrl");
	pr_info("bat authent cloudengine id:%s, config:%s, cloud en val %d\n", id, config, v);
	if (v == 1)
		chip_ctx->bat_authent_cloud_feedback = FEEDBACK_GOOD;

	return 0;
}
#endif

static bool get_bat_authent_support(struct fm1210 *chip_ctx)
{
	//struct this_platform_context *pf_ctx = NULL;
	int val;

	if (IS_ERR_OR_NULL(chip_ctx))
		return false;

	//pf_ctx = (struct this_platform_context *)chip_ctx->on_ctx.swi_ctx.pf_ctx;

#if IS_ENABLED(CONFIG_TRAN_TRANLOG)
	if (bat_authent_cloud_enable(chip_ctx))
		return false;
#endif

	//if (IS_ERR_OR_NULL(pf_ctx))
	//	return false;

	val = gpio_get_value(chip_ctx->board_id);
	pr_err("show board gpio val = %d\n", val);
	if (val == 0)
		return true;

	return false;
}

static enum power_supply_property ctx_psy_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
};

static int ctx_prop_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	return 0;
}

static int ctx_get_property(struct power_supply *psy, enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct fm1210 *chip_ctx = power_supply_get_drvdata(psy);
	int ret = 0;

	if (IS_ERR_OR_NULL(chip_ctx))
		return -EINVAL;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = get_bat_authent_support(chip_ctx);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int ctx_set_property(struct power_supply *psy,
			       enum power_supply_property prop,
			       const union power_supply_propval *val)
{
	return 0;
}

static void do_test_work(struct work_struct *work)
{
	struct fm1210 *chip_ctx = container_of(work,
					struct fm1210, test_work);
	u8 rbuf[146];
	u8 buf[146];

	//if (authon_test(&chip_ctx->on_ctx) != SDK_SUCCESS) {
	if (ON_SUCCESS) {
		pr_err("Device testing has failed.\n");
	}
	//echo 0200 > ow_para
	ow_set_tm((struct ow_tm_data *)ow_tm[2]);
	swi_protocol_hm_flag = 0;

	//init ow time
	ow_set_tm((struct ow_tm_data *)ow_tm[2]);
	ow_se_reset(chip_ctx);
	hm_error_cnt = 0;
	mdelay(10);
	if (ow_device_select(chip_ctx, CMD_OW_ADR) != 0)
		pr_err("fm1210 reset se fail\n");
	else
		pr_err("fm1210 reset se success\n");

	//cat ow_romid
	if (ow_read_block(chip_ctx, ROMID_ADDR, rbuf) != 0)
		pr_err("fm1210 get romid err\n");
	hex_to_str(buf, rbuf, READ_UID_SIZE);
	pr_err("fm1210 get romid = %s\n", buf + READ_UID_OFS);
	
	pr_err("fm1210 Device hard test suc.\n");
}
//PAGE1_ADDR 5,9,01
/*
9 year 1
10 mon 1 
11 12 dny
*/
#define ASCII_NUM_0		48
#define ASCII_NUM_9		57
#define ASCII_NUM_A		65
#define ASCII_NUM_Z		90
#define ASCII_NUM_A_OFFSET		55
#define ASCII_NUM_a		97
#define ASCII_NUM_z		122
#define ASCII_NUM_a_OFFSET		87
#define INT_NUM_10		10

static char ascii_to_char(char val)
{
	if (val >= ASCII_NUM_0 && val <= ASCII_NUM_9)
		return (val - ASCII_NUM_0);
	else if (val >= ASCII_NUM_A && val <= ASCII_NUM_Z)
		return (val - ASCII_NUM_A_OFFSET);
	else if (val >= ASCII_NUM_a && val <= ASCII_NUM_z)
		return (val - ASCII_NUM_a_OFFSET);
	else
		return val;
}

static int ctx_get_mac(struct fm1210 *chip_ctx,union com_propval *val)
{
	/* page1 : 1-block */
	u8 rbuf[128];
	u8 tbuf[256];
	//static char cbuf[128] = {NULL};
	char **pchar = val->ptr;
	int length = -EINVAL;
	//union com_propval get_val = {0};
	u8 ofs = 0;
	int i;

	ow_se_reset(chip_ctx);
	ow_device_select(chip_ctx, CMD_OW_ADR);
	mdelay(100);

	for (i = 0; i < PAGE1_BLOCK; i++) {
		if (ow_read_block(chip_ctx, PAGE1_ADDR + i, rbuf) != 0){
					goto out;	
				}
		memcpy(tbuf + ofs, rbuf, PAGE_BLOCK_SIZE);
		ofs += PAGE_BLOCK_SIZE;
	}

	length = sprintf(chip_ctx->mac, "%d%c-%02d-%c%c\n",(chip_ctx->base_year/10),tbuf[9],ascii_to_char(tbuf[10]),tbuf[11],tbuf[12]);

out:
	if(strlen(chip_ctx->mac) < 5){
		pr_err("ctx_get_mac!%d\n",tbuf[9]);
		return -EINVAL;
	}
    strncpy(*pchar, chip_ctx->mac, strlen(chip_ctx->mac));
    (*pchar)[strlen(chip_ctx->mac)] = '\0';
	pr_err("%s\n", *pchar);

	pr_err("%lu\n", strlen(chip_ctx->mac));
	return strlen(*pchar);
}

//shengchanshijian active_date//PAG6_ADDR
static int ctx_get_active_date(struct fm1210 *chip_ctx,union com_propval *val)
{
	/* page6 : 1-block */
	u8 rbuf[128];
	u8 tbuf[256];
	//char cbuf[128];
	char **pchar = val->ptr;
	int length;
	u8 ofs = 0;
	int i;
	ow_se_reset(chip_ctx);
	ow_device_select(chip_ctx, CMD_OW_ADR);
	mdelay(100);

	for (i = 0; i < PAGE6_BLOCK; i++) {
		if (ow_read_block(chip_ctx, PAGE6_ADDR + i, rbuf) != 0)
					goto out;
		memcpy(tbuf + ofs, rbuf, PAGE_BLOCK_SIZE);
		ofs += PAGE_BLOCK_SIZE;
	}

	length = sprintf(chip_ctx->active, "%c%c%c%c-%c%c-%c%c\n",tbuf[0],tbuf[1],tbuf[2],tbuf[3],tbuf[4],tbuf[5],tbuf[6],tbuf[7]);
out:
	if((strlen(chip_ctx->active) < 5) || (chip_ctx->active[0] == 0x30)){//(2)025 == 2 > 0
		return -EINVAL;
	}
    strncpy(*pchar, chip_ctx->active, strlen(chip_ctx->active));
    (*pchar)[strlen(chip_ctx->active)] = '\0';
	//*pchar = cbuf;
	pr_err("%s\n",*pchar);
	return strlen(*pchar);
}

static void update_bat_info_work(struct work_struct *work)
{
   /* page1 : 1-block */
     u8 rbuf[128];
     u8 tbuf[256];
     static int retry_cnt = 0;

     int length = -EINVAL;
     u8 ofs = 0;
     int i;
     struct fm1210 *chip_ctx = container_of(work, struct fm1210, update_work.work);

     ow_se_reset(chip_ctx);
     ow_device_select(chip_ctx, CMD_OW_ADR);
             mdelay(100);
     if(strlen(chip_ctx->mac) < 5) {
         for (i = 0; i < PAGE1_BLOCK; i++) {
             if (ow_read_block(chip_ctx, PAGE1_ADDR + i, rbuf) != 0){
                         goto out;
                     }
             memcpy(tbuf + ofs, rbuf, PAGE_BLOCK_SIZE);
			 ofs += PAGE_BLOCK_SIZE;
         }

         length = sprintf(chip_ctx->mac, "%d%c-%02d-%c%c\n",(chip_ctx->base_year/10),tbuf[9],ascii_to_char(tbuf[10]),tbuf[11],tbuf[12]);
		chip_ctx->mac[length] = '\0';
		memset(rbuf, 0, sizeof(rbuf));
		memset(tbuf, 0, sizeof(tbuf));
		ofs = 0;
	 }
     mdelay(100);
     if(strlen(chip_ctx->active) < 5) {
         for (i = 0; i < PAGE6_BLOCK; i++) {
             if (ow_read_block(chip_ctx, PAGE6_ADDR + i, rbuf) != 0)
                         goto out;
             memcpy(tbuf + ofs, rbuf, PAGE_BLOCK_SIZE);
			ofs += PAGE_BLOCK_SIZE;
         }

         length = sprintf(chip_ctx->active, "%c%c%c%c-%c%c-%c%c\n",tbuf[0],tbuf[1],tbuf[2],tbuf[3],tbuf[4],tbuf[5],tbuf[6],tbuf[7]);
		chip_ctx->active[length] = '\0';
	 }
     pr_err("%s success actice%s mac: %s\n",__func__, chip_ctx->active, chip_ctx->mac);
     return;
 out:
     pr_err("%s failed actice%s mac: %s\n",__func__, chip_ctx->active, chip_ctx->mac);
     if((strlen(chip_ctx->active) < 5) || (strlen(chip_ctx->mac) < 5)) {
         if(retry_cnt <= 3) {
             retry_cnt++;
             pr_err("fm1210  %d attempts\n", retry_cnt);
             schedule_delayed_work(&chip_ctx->update_work, msecs_to_jiffies(200));
         } else {
             retry_cnt = 0;
         }
     }
	return;
}
//PAGE7_ADDR
static int ctx_get_battery_health(struct fm1210 *chip_ctx,union com_propval *val)
{
	/* page1 : 1-block */
	u8 rbuf[128];
	u8 tbuf[256];
	static char cbuf[PAGE7_BLOCK * PAGE_BLOCK_SIZE];
	int length;
	char **pchar = val->ptr;
	u8 ofs = 0;
	int i;

	for (i = 0; i < PAGE7_BLOCK; i++) {
		if (ow_read_block(chip_ctx, PAGE7_ADDR + i, rbuf) != 0)
			return -EINVAL;
		memcpy(tbuf + ofs, rbuf, PAGE_BLOCK_SIZE);
		ofs += PAGE_BLOCK_SIZE;
	}

	length = sprintf(cbuf,"%s\n",tbuf);
	if(length < 0 || length >= 4){
			return -EINVAL;	
	}
    strncpy(*pchar, cbuf, length);
    (*pchar)[length + 1] = '\0';
	pr_err("%s\n",*pchar);
	//*buf = tbuf[0] - 0x30;
	return 0;
}
//PAGE8_ADDR
static int ctx_get_battery_cycle(struct fm1210 *chip_ctx,union com_propval *val)
{
	/* page1 : 1-block */
	u8 rbuf[128];
	u8 tbuf[256];
	static char cbuf[PAGE8_BLOCK * PAGE_BLOCK_SIZE];
	char **pchar = val->ptr;
	int length;
	u8 ofs = 0;
	int i;

	for (i = 0; i < PAGE8_BLOCK; i++) {
		if (ow_read_block(chip_ctx, PAGE8_ADDR + i, rbuf) != 0)
			return -EINVAL;
		memcpy(tbuf + ofs, rbuf, PAGE_BLOCK_SIZE);
		ofs += PAGE_BLOCK_SIZE;
	}
	length = sprintf(cbuf,"%s\n",tbuf);

	if(length < 0 || length >= sizeof(cbuf)){
			return -EINVAL;	
	}

    strncpy(*pchar, cbuf, length);
    (*pchar)[length + 1] = '\0';
	pr_err("ctx_get_battery_cycle:%s\n",*pchar);
	return 0;
}
static int tc_ctx_get_property(struct tran_device *dev,
				enum tran_common_prop prop,
				union com_propval *val)
{
	int ret;
	struct fm1210 *chip_ctx = tran_get_data(dev);

	if (IS_ERR_OR_NULL(chip_ctx))
		return -EINVAL;

	switch (prop) {
	case TRAN_PROP_BATT_MAC_DATA:
		ret = ctx_get_mac(chip_ctx,val);
		break;
	case TRAN_PROP_PHONE_ACTIVE_DATA:
		ret = ctx_get_active_date(chip_ctx,val);
		break;
	case TRAN_PROP_BATT_CYCLE:
		ret = ctx_get_battery_cycle(chip_ctx,val);
		break;
	case TRAN_PROP_BATT_HEALTH_LEVEL:
		ret = ctx_get_battery_health(chip_ctx,val);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}
//PAGE1_ADDR
static int ctx_set_mac(struct fm1210 *chip_ctx,const union com_propval *val)
{
	return 0;
}

//active_date 50901  int 65535  //PAGE6_ADDR
static int ctx_set_active_date(struct fm1210 *chip_ctx,const union com_propval *val)
{
	char **pchar = val->ptr;
	//char buf[128];
	u8 tbuf[256];
	u8 msg[PAGE6_BLOCK * PAGE_BLOCK_SIZE];
	u16 msglen = 0;
	u8 ofs = 0;
	int i ;

	/*if(*pchar == NULL){
		return -EINVAL;
	}*/
	msglen = snprintf(msg,sizeof(msg), "%s",*pchar);
	if(msglen > sizeof(msg) || msglen <= 2){
		return -EINVAL;
	}

	//str_to_hex(msg, buf, msglen);
	ofs = PAGE6_BLOCK * PAGE_BLOCK_SIZE - msglen;
	memcpy(tbuf, msg, msglen);
	if (ofs)
		memset(tbuf + msglen, 0, ofs);
	ow_ee_pump(chip_ctx, EE_PUMP_ACTIVE);
	ofs = 0;
	for (i = 0; i < PAGE6_BLOCK; i++) {
		if (ow_write_block(chip_ctx, PAGE6_ADDR + i, tbuf + ofs) != 0)
			return -EINVAL;
		ofs += PAGE_BLOCK_SIZE;
	}
	ow_ee_pump(chip_ctx, EE_PUMP_CLOSE);

	return 0;
}
//PAGE7_ADDR
static int ctx_set_battery_health(struct fm1210 *chip_ctx,const union com_propval *val)
{
	char **pchar = val->ptr;
	u8 tbuf[256];
	u8 msg[PAGE7_BLOCK * PAGE_BLOCK_SIZE];
	u16 msglen = 0;
	u8 ofs = 0;
	int i;

	msglen = snprintf(msg,sizeof(msg), "%s",*pchar);
	if(msglen > sizeof(msg)){
		return -EINVAL;
	}

	//str_to_hex(msg, buf, msglen);
	ofs = PAGE7_BLOCK * PAGE_BLOCK_SIZE - msglen;
	memcpy(tbuf, msg, msglen);
	if (ofs)
		memset(tbuf + msglen, 0, ofs);
	ow_ee_pump(chip_ctx, EE_PUMP_ACTIVE);
	ofs = 0;
	for (i = 0; i < PAGE7_BLOCK; i++) {
		if (ow_write_block(chip_ctx, PAGE7_ADDR + i, tbuf + ofs) != 0)
			return -EINVAL;
		ofs += PAGE_BLOCK_SIZE;
	}
	ow_ee_pump(chip_ctx, EE_PUMP_CLOSE);

	return 0;
}
////PAGE8_ADDR
static int ctx_set_battery_cycle(struct fm1210 *chip_ctx,const union com_propval *val)
{
	char **pchar = val->ptr;
	u8 tbuf[256];
	u8 msg[PAGE8_BLOCK * PAGE_BLOCK_SIZE];
	u16 msglen = 0;
	u8 ofs = 0;
	int i;

	msglen = snprintf(msg,sizeof(msg), "%s",*pchar);
	if(msglen > sizeof(msg)){
		return -EINVAL;
	}

	ofs = PAGE8_BLOCK * PAGE_BLOCK_SIZE - msglen;
	memcpy(tbuf, msg, msglen);
	if (ofs)
		memset(tbuf + msglen, 0, ofs);
	ow_ee_pump(chip_ctx, EE_PUMP_ACTIVE);
	ofs = 0;
	for (i = 0; i < PAGE8_BLOCK; i++) {
		if (ow_write_block(chip_ctx, PAGE8_ADDR + i, tbuf + ofs) != 0)
			return -EINVAL;
		ofs += PAGE_BLOCK_SIZE;
	}
	ow_ee_pump(chip_ctx, EE_PUMP_CLOSE);
	return 0;
}

static int tc_ctx_set_property(struct tran_device *dev,
					   enum tran_common_prop prop,
					   const union com_propval *val)
{
	struct fm1210 *chip_ctx = tran_get_data(dev);
	int ret;

	if (IS_ERR_OR_NULL(chip_ctx))
		return -EINVAL;

	if(!get_bat_authent_support(chip_ctx)){
		ret = -EINVAL;
		return ret;
	}

	switch (prop) {
	case TRAN_PROP_BATT_MAC_DATA:
		ret = ctx_set_mac(chip_ctx,val);
		break;
	case TRAN_PROP_PHONE_ACTIVE_DATA:
		ret = ctx_set_active_date(chip_ctx,val);
		break;
	case TRAN_PROP_BATT_CYCLE:
		ret = ctx_set_battery_cycle(chip_ctx,val);
		break;
	case TRAN_PROP_BATT_HEALTH_LEVEL:
		ret = ctx_set_battery_health(chip_ctx,val);
		break;

	default:
		ret = -EINVAL;
	}
	return ret;
}

static struct tran_ops ctx_psy1_ops = {
	.get_prop = tc_ctx_get_property,
	.set_prop = tc_ctx_set_property,
};

static int tc_charger_prop_init(struct fm1210 *chip_ctx)
{

	chip_ctx->tc_charger_props.alias_name = "authon_dev";
	tran_device_register("authon_dev",
						chip_ctx->dev, chip_ctx,
						&ctx_psy1_ops,
						&chip_ctx->tc_charger_props);

	return 0;
}

static int owse_probe(struct platform_device *pdev)
{
	int ret;
	struct fm1210 *chip_ctx;
	struct device_node *np = pdev->dev.of_node;
	/*BSP:added for EU battery information query by mingjian.chen 20250528 start*/
	struct pinctrl *swi_dev = NULL;
	struct pinctrl_state *swi_8mA = NULL;
	/*BSP:added for EU battery information query by mingjian.chen 20250528 end*/

	pr_err("%s\n", __func__);
	chip_ctx = devm_kzalloc(&pdev->dev, sizeof(*chip_ctx), GFP_KERNEL);
	if (!chip_ctx) {
		pr_err("kzalloc has failed.\n");
		ret = -ENOMEM;
		goto err;
	}

	if (!np) {
		pr_err("%s: no of node\n", __func__);
		return -ENODEV;
	}
	chip_ctx->dev = &pdev->dev;
	chip_ctx->board_id = of_get_named_gpio(np, "boardver", 0);
	if(gpio_is_valid(chip_ctx->board_id))
		pr_err("%s board_id_gpio = %d get succ\n", __func__, chip_ctx->board_id);
	ret = gpio_request(chip_ctx->board_id, "boardver");
	if (ret)
			pr_err("%s: board_id request failed\n", __func__);


	chip_ctx->dbg_gpio = of_get_named_gpio(np, "dbg-gpio", 0);
	if(gpio_is_valid(chip_ctx->dbg_gpio))
		pr_err("%s dbg_gpio = %d get succ\n", __func__, chip_ctx->dbg_gpio);
	ret = gpio_request(chip_ctx->dbg_gpio, "dbg-gpio");
	if (ret)
			pr_err("%s: dbg_gpio request failed\n", __func__);
	gpio_direction_output(chip_ctx->dbg_gpio, HIGH);

	chip_ctx->swi_gpio = of_get_named_gpio(np, "swi-gpio", 0);
	if(gpio_is_valid(chip_ctx->swi_gpio))
		pr_err("%s swi_gpio = %d get succ\n", __func__, chip_ctx->swi_gpio);

	if (chip_ctx->swi_gpio < 0) {
		pr_err("%s swi_gpio = %d get fail\n", __func__, chip_ctx->swi_gpio);
		ret = -ENODEV;
		goto err;
	} else {
		pr_err("%s swi_gpio = %d get succ\n", __func__, chip_ctx->swi_gpio);
		ret = gpio_request(chip_ctx->swi_gpio, "swi-gpio");
		if (ret) {
			pr_err("%s: swi_gpio request failed\n", __func__);
		} else {
			gpio_direction_output(chip_ctx->swi_gpio, HIGH);
		}
	}

	ret = of_property_read_u32(np, "base_year", &chip_ctx->base_year);
	if (ret || chip_ctx->base_year < 2000) {
		chip_ctx->base_year = 2020;
	}

	mutex_init(&chip_ctx->ow_access);
	/*BSP:added for EU battery information query by mingjian.chen 20250528 start*/	
	swi_dev = devm_pinctrl_get(&pdev->dev);
	
	if (IS_ERR_OR_NULL(swi_dev)) {
		pr_err("devm_pinctrl_get error.\n");
	} else {
		swi_8mA = pinctrl_lookup_state(swi_dev, "opt_authon_8mA");
		if (IS_ERR_OR_NULL(swi_8mA)) {
			pr_err("pinctrl_lookup_state error.\n");
		} else {
			ret = pinctrl_select_state(swi_dev, swi_8mA);
			if (ret < 0) {
				pr_err("pinctrl_select_state error. %d\n", ret);
			}
		}
	}
	/*BSP:added for EU battery information query by mingjian.chen 20250528 end*/


	if(get_bat_authent_support(chip_ctx)){
		tc_charger_prop_init(chip_ctx);
	}

	chip_ctx->psy_desc.name = "ctx_psy";
	chip_ctx->psy_desc.type = POWER_SUPPLY_TYPE_BATTERY;
	chip_ctx->psy_desc.properties = ctx_psy_props;
	chip_ctx->psy_desc.num_properties = ARRAY_SIZE(ctx_psy_props);
	chip_ctx->psy_desc.get_property = ctx_get_property;
	chip_ctx->psy_desc.set_property = ctx_set_property;
	chip_ctx->psy_desc.property_is_writeable = ctx_prop_is_writeable;
	chip_ctx->psy_cfg.drv_data = chip_ctx;
	chip_ctx->ctx_psy = devm_power_supply_register(chip_ctx->dev,
						&chip_ctx->psy_desc,
						&chip_ctx->psy_cfg);

	INIT_DELAYED_WORK(&chip_ctx->update_work, update_bat_info_work);
	INIT_WORK(&chip_ctx->test_work, do_test_work);
	schedule_work(&chip_ctx->test_work);
    schedule_delayed_work(&chip_ctx->update_work, msecs_to_jiffies(200));
#if IS_ENABLED(CONFIG_TRAN_TRANLOG)
	chip_ctx->regID = "1003000039";
	chip_ctx->bat_authent_cloud_feedback = NO_FEEDBACK;
	chip_ctx->bat_authent_notifier_block.notifier_call = bat_authent_notifier_call;
	register_cloudengine_notifier(chip_ctx->regID, "v1.x", &chip_ctx->bat_authent_notifier_block);
#endif

	dev_set_drvdata(&pdev->dev, chip_ctx);
	ret = sysfs_create_group(&pdev->dev.kobj, &ds_attr_group);
	if (ret)
		ds_dbg("Failed to register sysfs %d\n", ret);
	pr_err("fm1210 owse_probe succ\n");
	return 0;

err:
	pr_err("fm1210 owse_probe err\n");
	return ret;
}

static void owse_remove(struct platform_device *pdev)
{
	struct fm1210 *chip_ctx;

	chip_ctx = dev_get_drvdata(&pdev->dev);
	sysfs_remove_group(&pdev->dev.kobj, &ds_attr_group);
	gpio_direction_output(chip_ctx->swi_gpio, HIGH);
	gpio_free(chip_ctx->swi_gpio);

	gpio_direction_output(chip_ctx->dbg_gpio, HIGH);
	gpio_free(chip_ctx->dbg_gpio);
	mutex_destroy(&chip_ctx->ow_access);
	//gpio_direction_output(OWSE_LPC, HIGH);
	//gpio_free(OWSE_LPC);

	return;
}

static const struct of_device_id ow_dt_match[] = {
	{ .compatible = "fmsh,owse_ctl" },
	{ },
};

static struct platform_driver owse_driver = {
	.probe			= owse_probe,
	.remove			= owse_remove,
	.driver			= {
		.owner		= THIS_MODULE,
		.name		= "owse_ctl",
		.of_match_table = ow_dt_match,
	},
};

static int __init gpio_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&owse_driver);
	ds_dbg("init ret %x\n", ret);
	return ret;
}

static void __exit gpio_exit(void)
{
	platform_driver_unregister(&owse_driver);
	//sysfs_remove_group(my_kobj, &ds_attr_group);
	//kobject_put(my_kobj);
}

module_init(gpio_init);
module_exit(gpio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MXB");

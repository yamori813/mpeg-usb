/*
 * Copyright (c) 2018-2019 Hiroki Mori
 *
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#include "libusb.h"

#define MAXTANS 2

libusb_context *ctx = NULL;
libusb_device_handle *dev_handle;
int dumpfd;

static void read_callback(struct libusb_transfer *transfer)
{
	int res;

	if (transfer->actual_length) {
		write(dumpfd, transfer->buffer, transfer->actual_length);
	}
	res = libusb_submit_transfer(transfer);
}

int i2cwrite(libusb_device_handle *dev, int addr, int size, unsigned char *buf)
{
	int res = -1;
	unsigned char cmdbuf[64];
	unsigned char inbuf[64];
	int trns;

	cmdbuf[0] = 0x08;
	cmdbuf[1] = 0x44;
	cmdbuf[2] = size + 2;
	cmdbuf[3] = addr >> 8;
	cmdbuf[4] = addr & 0xff;
	memcpy(&cmdbuf[5], buf, size);
	res = libusb_bulk_transfer(dev, 0x01, cmdbuf, size + 5, &trns, 0);
	if (res != 0 || trns != size + 5)
		printf("i2cwrite error\n");
	res = libusb_bulk_transfer(dev, 0x81, inbuf, sizeof(inbuf), &trns, 0);
	if (res != 0 || trns != 1 || inbuf[0] != 8)
		printf("i2cwrite ack error\n");

	return res;
}

int i2cread(libusb_device_handle *dev, int addr, int size, unsigned char *buf)
{
	int res = -1;
	unsigned char cmdbuf[64];
	unsigned char inbuf[64];
	int trns;

	cmdbuf[0] = 0x09;
	cmdbuf[1] = 0x02;
	cmdbuf[2] = size;
	cmdbuf[3] = 0x44;
	cmdbuf[4] = addr >> 8;
	cmdbuf[5] = addr & 0xff;
	res = libusb_bulk_transfer(dev, 0x01, cmdbuf, 6, &trns, 0);
	if (res == 0) {
		res = libusb_bulk_transfer(dev, 0x81, inbuf, sizeof(inbuf), &trns, 0);
		if (res == 0 && trns == size + 1 && inbuf[0] == 8)
			memcpy(buf, inbuf + 1, size);
	}
	return res;
}

int readmem(libusb_device_handle *dev, int address)
{
	int res = -1;
	int timeout = 1000;
	unsigned char cmdbuf[8];
	unsigned char inbuf[256];
	int trns;

	cmdbuf[0] = 0x02;
	cmdbuf[1] = 0x00;
	cmdbuf[2] = 0x00;
	cmdbuf[3] = 0x00;
	cmdbuf[4] = 0x00;
	cmdbuf[5] = (address >> 16) & 0xff;
	cmdbuf[6] = (address >> 8) & 0xff;
	cmdbuf[7] = address & 0xff;
	res = libusb_bulk_transfer(dev, 0x01, cmdbuf, 8, &trns, timeout);
	if (res != 0 || trns != 8)
		printf("readmem address error\n");
#if DEBUG
	printf("w %d %d\n", res, trns);
#endif
	res = libusb_bulk_transfer(dev, 0x81, inbuf, sizeof(inbuf), &trns, timeout);
	if (res != 0 || trns != 4)
		printf("readmem data error\n");
#if DEBUG
	printf("r %d %d %02x %02x %02x %02x\n", res, trns, inbuf[0], inbuf[1], inbuf[2], inbuf[3]);
#endif
	return (inbuf[3] << 24) | (inbuf[2] << 16) | (inbuf[1] << 8) | inbuf[0];
}

int writemems(libusb_device_handle *dev, int address, int *data, int count)
{
	int res = -1;
	int timeout = 1000;
	unsigned char cmdbuf[64];
	unsigned char inbuf[256];
	int trns;
	int i, next, pos;

	pos = 0;
	do {
		if (count > 8)
			next = 8;
		else
			next = count;
		cmdbuf[0] = 0x01;
		for (i = 0; i < next; ++i) {
			cmdbuf[1 + 7 * i] = data[pos] & 0xff;
			cmdbuf[2 + 7 * i] = (data[pos] >> 8) & 0xff;
			cmdbuf[3 + 7 * i] = (data[pos] >> 16) & 0xff;
			cmdbuf[4 + 7 * i] = (data[pos] >> 24) & 0xff;
			cmdbuf[5 + 7 * i] = (address >> 16) & 0xff;
			cmdbuf[6 + 7 * i] = (address >> 8) & 0xff;
			cmdbuf[7 + 7 * i] = address & 0xff;
			++pos;
			--count;
			++address;
		}
		res = libusb_bulk_transfer(dev, 0x01, cmdbuf, 7 * next + 1,
		    &trns, timeout);
		if (res != 0 || trns != 7 * next + 1)
			printf("writemem error\n");
	} while (count != 0);

#if DEBUG
	printf("w %d %d\n", res, trns);
#endif
	return res;
}

int writemem(libusb_device_handle *dev, int address, int data)
{
	return writemems(dev, address, &data, 1);
}

int readreg(libusb_device_handle *dev, int address)
{
	int res = -1;
	int timeout = 1000;
	unsigned char cmdbuf[8];
	unsigned char inbuf[256];
	int trns;

	cmdbuf[0] = 0x05;
	cmdbuf[1] = 0x00;
	cmdbuf[2] = 0x00;
	cmdbuf[3] = 0x00;
	cmdbuf[4] = 0x00;
	cmdbuf[5] = 0x00;

	cmdbuf[6] = address >> 8;
	cmdbuf[7] = address & 0xff;
	res = libusb_bulk_transfer(dev, 0x01, cmdbuf, 8, &trns, timeout);
	if (res != 0)
		printf("readreg address error %d\n", res);
#if DEBUG
	printf("w %d %d\n", res, trns);
#endif
	res = libusb_bulk_transfer(dev, 0x81, inbuf, sizeof(inbuf), &trns, timeout);
	if (res != 0 || trns != 4)
		printf("readreg data error\n");
#if DEBUG
	printf("r %d %d %02x %02x %02x %02x\n", res, trns, inbuf[0], inbuf[1], inbuf[2], inbuf[3]);
#endif
	return inbuf[3] << 24 | inbuf[2] << 16 | inbuf[1] << 8 | inbuf[0];
}

void writereg(libusb_device_handle *dev, int address, unsigned int val)
{
	int res = -1;
	int timeout = 1000;
	unsigned char cmdbuf[8];
	int trns;
	
	cmdbuf[0] = 0x04;
	cmdbuf[1] = val & 0xff;
	cmdbuf[2] = (val >> 8) & 0xff;
	cmdbuf[3] = (val >> 16) & 0xff;
	cmdbuf[4] = (val >> 24) & 0xff;
	cmdbuf[5] = 0x00;
	cmdbuf[6] = address >> 8;
	cmdbuf[7] = address & 0xff;
	res = libusb_bulk_transfer(dev, 0x01, cmdbuf, 8, &trns, timeout);
	if (res !=0)
		printf("writereg error\n");
#if DEBUG
	printf("w %d %d\n", res, trns);
	printf("%d\n", res);
#endif
}

void gpio_dir(libusb_device_handle *dev, unsigned int mask, int unsigned val)
{
	int reg;
	if (~mask) {
		reg = readreg(dev, 0x9020);
		reg = (reg & ~mask) | (val & mask);
	} else {
		reg = val;
	}
	writereg(dev, 0x9020, reg);
}

void gpio_out(libusb_device_handle *dev, unsigned int mask, unsigned int val)
{
	int reg;

	if (~mask) {
		reg = readreg(dev, 0x900c);
		reg = (reg & ~mask) | (val & mask);
	} else {
		reg = val;
	}
	writereg(dev, 0x900c, reg);
}


int enccmd(libusb_device_handle *dev, int *data, int size)
{
	int i;
	int ret;
	int maxtry = 1000;

	writemems(dev, 0x44, data, size);
	writemem(dev, 0x44, 0x02 | 0x01);
	i = 0;
	do {
		ret = readmem(dev, 0x44);
		++i;
		if (i == maxtry)
			break;
		usleep(100);
	} while (!(ret & 0x04));
	if (i == maxtry)
		return -1;

	ret = readmem(dev, 0x46);
	if (ret != 0) {
		printf("MORI MORI enccmd error %d\n", ret);
		return -1;
	}

	return 0;
}

void mkcmd(int *data, int cmd, int *para, int size)
{
	int i;

	i = 0;
	data[i++] = 0;
	data[i++] = cmd;   // command
	data[i++] = 0;
	data[i++] = 0x00060000;   // timeout
	for (;i < 16; ++i) {
		if (size != 0) {
			data[i] = para[i-4];
			--size;
		} else {
			data[i] = 0;
		}
	}
}

void pingenc(libusb_device_handle *dev)
{
	int data[16];

	mkcmd(data, 0x80, NULL, 0);   // CX2341X_ENC_PING_FW
	if (enccmd(dev, data, sizeof(data)) == 0)
		printf("CX23416 arrive\n");
	else
		printf("CX23416 error\n");
}

void getverenc(libusb_device_handle *dev)
{
	int data[16];
	int res;

	mkcmd(data, 0xc4, NULL, 0);   // CX2341X_ENC_GET_VERSION
	if (enccmd(dev, data, sizeof(data)) == 0) {
		res = readmem(dev, 0x48);
		printf("CX23416 firmware version %d.%d.%d\n",
		    res >> 24, (res >> 16) & 0xff, res & 0xffff);
	}
	else
		printf("get version error\n");
}

void confenc(libusb_device_handle *dev, int sample)
{
	int data[16];
	int para[16];
	int i;
	int res;

	para[0] = 1;
	para[1] = 0;
	mkcmd(data, 0xbb, para, 2);   // CX2341X_ENC_SET_OUTPUT_PORT
	if (enccmd(dev, data, sizeof(data)) != 0)
		printf("confenc error\n");

	para[0] = 7;
	para[1] = 0x0190;
	mkcmd(data, 0xc7, para, 2);   // CX2341X_ENC_SET_PGM_INDEX_INFO
	if (enccmd(dev, data, sizeof(data)) != 0)
		printf("confenc error\n");

	para[0] = 0x140;
	para[1] = 0x140;
	mkcmd(data, 0xd6, para, 2);   // CX2341X_ENC_SET_NUM_VSYNC_LINES
	if (enccmd(dev, data, sizeof(data)) != 0)
		printf("confenc error\n");

	para[0] = 0;
	para[1] = 0;
	para[2] = 0x10000000;
	para[3] = 0xffffffff;
	mkcmd(data, 0xd5, para, 4);   // CX2341X_ENC_SET_EVENT_NOTIFICATION
	if (enccmd(dev, data, sizeof(data)) != 0)
		printf("confenc error\n");

	para[0] = 0;
	mkcmd(data, 0x8f, para, 1);   // CX2341X_ENC_SET_FRAME_RATE
	if (enccmd(dev, data, sizeof(data)) != 0)
		printf("confenc error\n");

	para[0] = 480;
	para[1] = 720;
	mkcmd(data, 0x91, para, 2);   // CX2341X_ENC_SET_FRAME_SIZE
	if (enccmd(dev, data, sizeof(data)) != 0)
		printf("confenc error\n");

	para[0] = 2;
	mkcmd(data, 0x99, para, 1);   // CX2341X_ENC_SET_ASPECT_RATIO
	if (enccmd(dev, data, sizeof(data)) != 0)
		printf("confenc error\n");

	para[0] = 0xffffffff;
	para[1] = 0;
	para[2] = 0;
	para[3] = 0;
	para[4] = 0;
	mkcmd(data, 0xb7, para, 5);   // CX2341X_ENC_SET_VBI_LINE
	if (enccmd(dev, data, sizeof(data)) != 0)
		printf("confenc error\n");

	para[0] = 0;
	mkcmd(data, 0xb9, para, 1);   // CX2341X_ENC_SET_STREAM_TYPE
	if (enccmd(dev, data, sizeof(data)) != 0)
		printf("confenc error\n");

	para[0] = 0;
	para[1] = 6000000;
	para[2] = 8000000 / 400;
	para[3] = 0;
	para[4] = 0;
	para[5] = 0;
	mkcmd(data, 0x95, para, 6);   // CX2341X_ENC_SET_BIT_RATE
	if (enccmd(dev, data, sizeof(data)) != 0)
		printf("confenc error\n");

	para[0] = 0x0f;
	para[1] = 0x03;
	mkcmd(data, 0x97, para, 2);   // CX2341X_ENC_SET_GOP_PROPERTIES
	if (enccmd(dev, data, sizeof(data)) != 0)
		printf("confenc error\n");

	para[0] = 0x00;
	mkcmd(data, 0xb1, para, 1);   // CX2341X_ENC_SET_3_2_PULLDOWN
	if (enccmd(dev, data, sizeof(data)) != 0)
		printf("confenc error\n");

	para[0] = 0x00;
	mkcmd(data, 0xc5, para, 1);   // CX2341X_ENC_SET_GOP_CLOSURE
	if (enccmd(dev, data, sizeof(data)) != 0)
		printf("confenc error\n");

	switch(sample) {
		case 32000:
			para[0] = 0x40b8 | 2;   // 32K
			break;
		case 44100:
			para[0] = 0x40b8 | 0;   // 44.1K
			break;
		case 48000:
			para[0] = 0x40b8 | 1;   // 48K
			break;
	}
	mkcmd(data, 0xbd, para, 1);   // CX2341X_ENC_SET_AUDIO_PROPERTIES
	if (enccmd(dev, data, sizeof(data)) != 0)
		printf("confenc error\n");

	para[0] = 1;
	para[1] = 1;
	mkcmd(data, 0xa1, para, 2);   // CX2341X_ENC_SET_SPATIAL_FILTER_TYPE
	if (enccmd(dev, data, sizeof(data)) != 0)
		printf("confenc error\n");

	mkcmd(data, 0xcd, NULL, 0);   // CX2341X_ENC_INITIALIZE_INPUT
	if (enccmd(dev, data, sizeof(data)) != 0)
		printf("confenc error\n");
}

void preconfenc(libusb_device_handle *dev)
{
	int data[16];
	int para[16];
	int i;
	int res;

	para[0] = 3;
	para[1] = 1;
	para[2] = 0;
	para[3] = 0;
	mkcmd(data, 0xdc, para, 4);
	if (enccmd(dev, data, sizeof(data)) != 0)
		printf("preconfenc error\n");

	para[0] = 8;
	para[1] = 0;
	para[2] = 0;
	para[3] = 0;
	mkcmd(data, 0xdc, para, 4);
	if (enccmd(dev, data, sizeof(data)) != 0)
		printf("preconfenc error\n");

	para[0] = 0;
	para[1] = 3;
	para[2] = 0;
	para[3] = 0;
	mkcmd(data, 0xdc, para, 4);
	if (enccmd(dev, data, sizeof(data)) != 0)
		printf("preconfenc error\n");

	para[0] = 15;
	para[1] = 0;
	para[2] = 0;
	para[3] = 0;
	mkcmd(data, 0xdc, para, 4);
	if (enccmd(dev, data, sizeof(data)) != 0)
		printf("preconfenc error\n");

	para[0] = 4;
	para[1] = 1;
	mkcmd(data, 0xdc, para, 2);
	if (enccmd(dev, data, sizeof(data)) != 0)
		printf("preconfenc error\n");
}

void startenc(libusb_device_handle *dev)
{
	int data[16];
	int para[16];
	int i;

	para[0] = 0;
	mkcmd(data, 0xd9, para, 1);
	if (enccmd(dev, data, sizeof(data)) != 0)
		printf("mute video error\n");

	para[0] = 0;
	para[1] = 0x13;
	mkcmd(data, 0x81, para, 2);
	if (enccmd(dev, data, sizeof(data)) == 0)
		printf("CX23416 start\n");
	else
		printf("CX23416 start error\n");
}

void dumpallreg(libusb_device_handle *dev_handle)
{
	int i;
	int res;

	for (i = 0;i < 0x100; i += 4) {
		res = readreg(dev_handle, i);
		printf("%04x %08x\n", i, res);
	}
	for (i = 0;i < 0x100; i += 4) {
		res = readreg(dev_handle, i + 0x0700);
		printf("%04x %08x\n", i + 0x0700, res);
	}
	for (i = 0;i <= 0x54; i += 4) {
		res = readreg(dev_handle, i + 0x9000);
		printf("%x %08x\n", i + 0x9000, res);
	}
	for (i = 0;i <= 0x6c; i += 4) {
		res = readreg(dev_handle, i + 0xa000);
		printf("%x %08x\n", i + 0xa000, res);
	}
	for (i = 0;i <= 0x20; i += 4) {
		res = readreg(dev_handle, i + 0xaa00);
		printf("%x %08x\n", i + 0xaa00, res);
	}
}

void
signalhandler(int signum)
{
	close(dumpfd);

	libusb_release_interface(dev_handle, 0);

	libusb_close(dev_handle);

	libusb_exit(ctx);

	exit(1);
}

void inputsel(libusb_device_handle *dev, int in)
{
	unsigned char cmdbuf[128];
	unsigned char buf[1024];
	int addr;

	printf("Input is %s\n", in == 0 ? "Composite" : "S-Video");

	// Video Input Control
	addr = 0x103;
	cmdbuf[0] = in == 0 ? 0x00 : (1 | (2 << 6));
	i2cwrite(dev_handle, addr, 1, cmdbuf);

	// Video Mode Control 2
	addr = 0x401;
	i2cread(dev_handle, addr, 1, buf);
	cmdbuf[0] = (buf[0] & ~0x06) | (in == 0 ? 0 : (1 << 1));
	i2cwrite(dev_handle, addr, 1, cmdbuf);

	// AFE Control 2
	addr = 0x105;
	i2cread(dev_handle, addr, 1, buf);
	cmdbuf[0] = (buf[0] & ~0x0e) | (in == 0 ? 0 : (6 << 1));
	i2cwrite(dev_handle, addr, 1, cmdbuf);
}

void audioinit(libusb_device_handle *dev, int sample)
{
	unsigned char cmdbuf[128];
	int addr;
	int pll_frac, pll_int, pll_post;
	double pll_tmp;

	/*
	  PLL calucrate documented at datasheet 3.15.
	  But I can't calucrate pll_frac by this page method.
	  I use fixed value.
	 */

	printf("Set Audio Sampling %d\n", sample);
	switch (sample) {
		case 32000:
			pll_int = 0x08;
			pll_post = 0x1e;
			pll_frac = 0x012a0869;
			break;
		case 44100:
			pll_int = 0x09;
			pll_post = 0x18;
			pll_frac = 0x00ec6bd6;
			break;
		case 48000:
			pll_int = 0x0a;
			pll_post = 0x18;
			pll_frac = 0x0098d6e5;
			break;
	}
	printf("Aux PLL Fractional %x\n", pll_frac);

	addr = 0x108;
	cmdbuf[0] = 0x0f;
	cmdbuf[1] = 0x04;
	cmdbuf[2] = pll_int;
	cmdbuf[3] = pll_post;
	i2cwrite(dev_handle, addr, 4, cmdbuf);

	addr = 0x110;
	cmdbuf[0] = pll_frac & 0xff;
	cmdbuf[1] = (pll_frac >> 8) & 0xff;
	cmdbuf[2] = (pll_frac >> 16) & 0xff;
	cmdbuf[3] = (pll_frac >> 24) & 0xff;
	i2cwrite(dev_handle, addr, 4, cmdbuf);

	addr = 0x127;
	cmdbuf[0] = (1 << 6) | (int)( 256.0/384.0 * pll_post);
	i2cwrite(dev_handle, addr, 1, cmdbuf);
	printf("Pin Configuration 12 %02x\n", cmdbuf[0]);
}

void swapbyte(unsigned char *buf, int size)
{
	int i;
	unsigned a, b, c, d;

	for (i = 0;i < size; ++i) {
		a = buf[i * 4];
		b = buf[i * 4 + 1];
		c = buf[i * 4 + 2];
		d = buf[i * 4 + 3];
		buf[i * 4 + 3] = a;
		buf[i * 4 + 2] = b;
		buf[i * 4 + 1] = c;
		buf[i * 4] = d;
	}
}

int main(int argc, char *argv[])
{
	libusb_device **devs;
	libusb_device *udev;
	struct libusb_device_descriptor desc;
	unsigned char strDesc[256];
	int r, cnt;
	unsigned char cmdbuf[128];
	unsigned char buf[1024];
	int trns;
	int res;
	int addr;
	int mbox;
	int i;
	int vid, pid;
	int reg;
	int input = 1;
	int sample = 48000;
	int ch;
	extern int optind;

	while ((ch = getopt(argc, argv, "34c")) != -1)
		switch (ch) {
			case 'c':
				input = 0;
				break;
			case '3':
				sample = 32000;
				break;
			case '4':
				sample = 44100;
				break;
		}

	argc -= optind;
	argv += optind;

	if (argc != 2) {
		printf("usage: mpegcapt [-c] <cx firmware> <output mpeg file>\n");
		exit(-1);
	}

	signal(SIGINT, signalhandler);

	r = libusb_init(&ctx);

	cnt = libusb_get_device_list(ctx, &devs);

	vid = -1;
	for (i = 0; i < cnt; ++i) {
		r = libusb_get_device_descriptor(devs[i], &desc);
		if ((desc.idVendor == 0x04bb && desc.idProduct == 0x0516) ||
		    (desc.idVendor == 0x0411 && desc.idProduct == 0x0065)) {
			vid = desc.idVendor;
			pid = desc.idProduct;
			break;
		}
	}
	if (vid == -1) {
		printf("device not found\n");
		libusb_exit(ctx);
		exit(1);
	}


	dev_handle = libusb_open_device_with_vid_pid(ctx, vid, pid);

	libusb_claim_interface(dev_handle, 0);

	r = libusb_get_string_descriptor_ascii(dev_handle,
	    desc.iProduct, strDesc, 256);
	printf("found %s\n", strDesc);

	cmdbuf[0] = 0xde;   // Encoder resume
	res = libusb_bulk_transfer(dev_handle, 0x01, cmdbuf, 1, &trns, 0);

	cmdbuf[0] = 0x0b;
	res = libusb_bulk_transfer(dev_handle, 0x01, cmdbuf, 1, &trns, 0);
	if (res == 0) {
		res = libusb_bulk_transfer(dev_handle, 0x81, buf, 1024,
		    &trns, 0);
		if (res == 0 && buf[0] == 0x80)
			printf("Device has hi speed mode\n");
	}

	cmdbuf[0] = 0xeb;
	res = libusb_bulk_transfer(dev_handle, 0x01, cmdbuf, 1, &trns, 0);
	if (res == 0) {
		res = libusb_bulk_transfer(dev_handle, 0x81, buf, 1024,
		    &trns, 0);
		if (res == 0) {
			if (buf[0] == 0)
				printf("Device don't have eeprom\n");
			else
				printf("Device eeprom address is %x\n", buf[0]);
		}
	}

	addr = 0x100;
	i2cread(dev_handle, addr, 2, buf);
	printf("I2C read Device ID(%x) %x %x\n", addr,
	    buf[0], buf[1]);
	printf("Encoder Chip is CX25%2x%1d Rev %d\n", buf[1], buf[0] >> 4,
	    buf[0] & 0xf);

	addr = 0x40d;
	i2cread(dev_handle, addr, 1, buf);
	printf("I2C read Video Decoder Core General Status 1(%x) %x\n", addr, buf[0]);

	addr = 0x40e;
	i2cread(dev_handle, addr, 1, buf);
	printf("I2C read Video Decoder Core General Status 2(%x) %x\n", addr, buf[0]);

	addr = 0x128;
	i2cread(dev_handle, addr, 3, buf);
	printf("VID_COUNT %06x\n", (buf[2] << 16) | (buf[1] << 8) | buf[0]);

	addr = 0x12c;
	i2cread(dev_handle, addr, 3, buf);
	printf("AUD_COUNT %06x\n", ((buf[2] & 0xf) << 16) | (buf[1] << 8) | buf[0]);
	printf("AUD_LOCK_FREQ_SHIFT %d\n", buf[2] >> 4);

	writereg(dev_handle, 0x0048, 0xffffffff);

	gpio_dir(dev_handle, 0xffffffff,0x00000088);
	gpio_out(dev_handle, 0xffffffff,0x00000008);

	cmdbuf[0] = 0xdd;
	res = libusb_bulk_transfer(dev_handle, 0x01, cmdbuf, 1, &trns, 0);

	writereg(dev_handle, 0xa064, 0x00000000);
	gpio_dir(dev_handle, 0xffffffff,0x00000408);
	gpio_out(dev_handle, 0xffffffff,0x00000008);
	writereg(dev_handle, 0x9058, 0xffffffed);
	writereg(dev_handle, 0x9054, 0xfffffffd);
	writereg(dev_handle, 0x07f8, 0x80000800);
	writereg(dev_handle, 0x07fc, 0x0000001a);
	writereg(dev_handle, 0x0700, 0x00000000);
	writereg(dev_handle, 0xaa00, 0x00000000);
	writereg(dev_handle, 0xaa04, 0x00057810);
	writereg(dev_handle, 0xaa10, 0x00148500);
	writereg(dev_handle, 0xaa18, 0x00840000);

	cmdbuf[0] = 0x52;
	res = libusb_bulk_transfer(dev_handle, 0x01, cmdbuf, 1, &trns, 0);

	cmdbuf[0] = 0x06;
	cmdbuf[1] = 0;
	res = libusb_bulk_transfer(dev_handle, 0x01, cmdbuf, 2, &trns, 0);

	unsigned char data[0x8000];
	int fd = open(argv[0], O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "** Couldn't open file for reading: %s\n",
		    argv[0]);
		exit(-1);
	} else {
		ssize_t n;
		while((n = read(fd, data, 0x8000)) != 0) {
			swapbyte(data, 0x8000 / 4);
			res = libusb_bulk_transfer(dev_handle, 0x02,
			    (unsigned char *)data, 0x8000, &trns, 0);
			if (res != 0 || trns != 0x8000)
				printf("CX23416 firmware down load error\n");
		}
		close(fd);
	}
	printf("CX23416 firmware downloaded\n");

	writereg(dev_handle, 0x9054, 0xffffffff);
	writereg(dev_handle, 0x9058, 0xffffffe8);

	cmdbuf[0] = 0x06;
	cmdbuf[1] = 0;
	res = libusb_bulk_transfer(dev_handle, 0x01, cmdbuf, 2, &trns, 0);

	pingenc(dev_handle);
	getverenc(dev_handle);

	writereg(dev_handle, 0x0048, 0xbfffffff);

	preconfenc(dev_handle);
	confenc(dev_handle, sample);
	startenc(dev_handle);

	inputsel(dev_handle, input);

	audioinit(dev_handle, sample);

	// Pin Control 2
	addr = 0x115;
	cmdbuf[0] = 0x0c;
	i2cwrite(dev_handle, addr, 1, cmdbuf);

	// Pin Control 3
	addr = 0x116;
	cmdbuf[0] = 1 << 2;   // PLL_CLK_OUT_EN for Audio
	i2cwrite(dev_handle, addr, 1, cmdbuf);

	printf("video decoder start\n");

	unsigned char * mpeg;
	struct libusb_transfer *transfer[MAXTANS];
	for (i = 0; i < MAXTANS; ++i) {
		mpeg = (unsigned char *)malloc(0x20000);
		transfer[i] = libusb_alloc_transfer(0);
		libusb_fill_bulk_transfer(transfer[i], dev_handle, 0x84,
		    mpeg, 0x20000, read_callback, NULL, 1000);
	}
	for (i = 0; i < MAXTANS; ++i)
		libusb_submit_transfer(transfer[i]);

	cmdbuf[0] = 0x36;
	res = libusb_bulk_transfer(dev_handle, 0x01, cmdbuf, 1, &trns, 0);
	if (res == 0)
		printf("usb stream start\n");
	else
		printf("usv stream start error\n");

	dumpfd = open(argv[1], O_RDWR | O_CREAT, 0644);

	while (1)
		libusb_handle_events(ctx);

	/* not reach hier */

}


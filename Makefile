mpegcapt: mpegcapt.c
	cc -o mpegcapt mpegcapt.c -I../libusb1 -L../libusb1 -l usb

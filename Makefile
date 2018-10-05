#
#
#

LIBUSBDIR=.

mpegcapt: mpegcapt.c
	cc -o mpegcapt mpegcapt.c -I$(LIBUSBDIR) -L$(LIBUSBDIR) -l usb

clean:
	rm -f mpegcapt

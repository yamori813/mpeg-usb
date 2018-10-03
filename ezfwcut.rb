#!/usr/local/bin/ruby
#
# EZ-USB Firmware 
#

#sysfile="win_driver/p2usbwdm.sys"
sysfile=ARGV[0]

p sysfile

bin = open(sysfile)
bin.binmode
pad = 0
offset = 0
lastend = 0
while (ch = bin.read(1))
  nu = ch.ord
  if nu == 0xaa then
    pad = pad + 1
  else
    if pad > 16 then
      start = offset - 0x10000
      if lastend <= start then
        print "dd if=" + sysfile + " of=ez" + start.to_s(16) + ".bin" + " skip=0x" + start.to_s(16) + " bs=1 count=0x2000\n"
      end
      lastend = offset
    end
  pad = 0
  end
  offset = offset + 1
end

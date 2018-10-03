#
# CX Firmware 
# find a7 0d 00 00 66 bb 55 aa

#sysfile="win_driver/p2usbwdm.sys"
sysfile=ARGV[0]


patern = [0x00, 0x00, 0x66, 0xbb, 0x55, 0xaa]
bin = open(sysfile)
bin.binmode
offset = 0
find = 0
while (ch = bin.read(1))
  nu = ch.ord
  if patern[find] == nu then
    find = find + 1
  else
    find = 0
  end
  if find == 6 then
    start = offset - 7
        print "dd if=" + sysfile + " of=cx" + start.to_s(16) + ".bin" + " skip=0x" + start.to_s(16) + " bs=1 count=0x40000\n"
    find = 0
  end
  offset = offset + 1
end

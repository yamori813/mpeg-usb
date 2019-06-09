#!/usr/bin/ruby
#
# EZ-USB Firmware 
# find 0xaa padding at end of firmware

if ARGV.size() != 1 then
  print "Usage ezfwcut.rb <windows sys file>\n"
elsif File.file?(ARGV[0]) == false then
  print "File not found\n"
else
  bin = open(ARGV[0])
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
          bin.seek(-0x10001, IO::SEEK_CUR)
          print "dump to ez" + start.to_s(16) + ".bin\n"
          fw = open("ez" + start.to_s(16) + ".bin", "wb")
          fw.write(bin.read(0x2000))
          fw.close
          bin.seek(0x10000 - 0x2000 + 1, IO::SEEK_CUR)
        end
        lastend = offset
      end
    pad = 0
    end
    offset = offset + 1
  end
end

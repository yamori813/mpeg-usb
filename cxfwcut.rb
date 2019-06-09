#!/usr/bin/ruby
#
# CX Firmware 
# find 00 00 66 bb 55 aa

if ARGV.size() != 1 then
  print "Usage ezfwcut.rb <windows sys file>\n"
elsif File.file?(ARGV[0]) == false then
  print "File not found\n"
else
  patern = [0x00, 0x00, 0x66, 0xbb, 0x55, 0xaa]
  bin = open(ARGV[0])
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
    if find == patern.length then
      start = offset - 7
      print("dump to cx" + start.to_s(16) + ".bin\n")
      bin.seek(-8, IO::SEEK_CUR)
      fw = open("cx" + start.to_s(16) + ".bin", "wb")
      fw.write(bin.read(0x40000))
      fw.close
      find = 0
      offset = offset + 0x40000 - 7
    else
      offset = offset + 1
    end
  end
end

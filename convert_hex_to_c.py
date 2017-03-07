#!/usr/bin/python

import sys

if len(sys.argv) != 2:
  print "Usage: convert filename.hex"
  sys.exit(-1)

filename = sys.argv[1]

f = open(filename, "r");

print "// Paste this output into the hexcode field of your image_t structure."
print "{"
print "  // octet representation of %s." % filename
for line in f.readlines():
  line = line.strip()
  c = 1
  while c < len(line):
    # Skip Extended Linear Address Records
    if not line.startswith(':04'):
      sys.stdout.write("0x%s%s, " % (line[c], line[c + 1]))
    c = c + 2
  print
print "}"

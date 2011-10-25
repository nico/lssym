#!/usr/bin/env python

"""Given a list of key/value pairs, outputs the keys with multiple values.

For example, ardump output lines looking like this

  __ZN8autofill6kFaxReE autofill_regex_constants.o
  __ZN8autofill7kCityReE autofill_regex_constants.o
  __ZN8autofill7kNameReE autofill_regex_constants.o
  __ZN8autofill7kZip4ReE autofill_regex_constants.o
  __ZN8autofill8kEmailReE autofill_regex_constants.o

Piping ardump's output into find_dupes will print all symbols that are defined
in multiple .o files (usually, these will be inline functions. To find 'real'
duplicate functions, run `ld -all_load libbrowser.a`):

    ./ardump libbrowser.a  | python find_dupes.py 
"""
import collections
import fileinput

symbols = collections.defaultdict(list)
for line in fileinput.input():
  symbol, o_file = line.split()
  symbols[symbol].append(o_file)

for symbol, o_files in symbols.iteritems():
  if len(o_files) > 1:
    print symbol, 'defined in:'
    for o_file in o_files:
      print ' ', o_file

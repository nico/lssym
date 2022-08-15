A toy program to teach myself more about the mach-o file format. Right now,
it roughly does what `dsymutil -s` does (which is a bit more than `nm`), but
worse.

Demo
----

But one can already see interesting things with this tool.

See symbols in `lssym` (like with `nm`, but with some `otool -l` thrown in): 

    clang -o lssym lssym.c -Wall 
    ./lssym lssym

See what's left after stripping:

    strip lssym
    ./lssym lssym

(Compare with the output of `nm lssym` and `dsymutil -s lssym` in both cases.
Note that `strip` adds a symbol that looks like

    radr://5614542    N_STAB 3c    n_sect 000 n_desc 0x0000    n_value 0x5614542

I wonder what that bug is. `nm` doesn't list it because `nm` only lists symbols,
and this is a stabs debug info entry which `nm` only shows if you pass `-a`.)

Now build with debug info:

    clang -o lssym lssym.c -Wall -gdwarf-2
    ./lssym lssym

Note that the file now includes stabs debug info, even though we asked for
dwarf. `dsymutil lssym` complains about a temporary `.o` file. Let's break up
compile and link step:

    clang -c lssym.c -Wall -gdwarf-2
    clang -o lssym lssym.o
    dsymutil lssym
    ./lssym lssym         # Still contains stabs entries.
    dwarfdump lssym.dSYM  # But this finds dwarf info now, too.
    dwarfdump lssym.o     # So does this.
    dwarfdump lssym       # This doesn't.

If you don't want `ld` to write stabs info (since you have a dSYM, it's
pointless, and might slow down the `ld` step), link like this:

    clang -o lssym lssym.o -Wl,-S

If you pass `-Wl,-s` instead, `ld` will claim that `-s` is obsolete and ignored,
but still still strip all symbols that can be stripped (after first writing them
as far as I understand -- so that probably doesn't help with linker
performance).


Related tools
-------------

* `dyldinfo` can show relocation info (`man dyldinfo` works, but dyldinfo isn't
  in my path for some reason -- `xcrun dyldinfo` finds it though)
  - e.g. -exports, -opcodes, ...
* `dyld_info` is apparently different from `dyldinfo`, and can print chained
  relocs, dependent librarys, opcodes, ... (and it doesn't need `xcrun`)
* `dwarfdump` shows dwarf debug data
* `dsymutil` can show symbol tables (`-s`) and link dwarf debug info from .o
  files into a single .dSYM file.
* `nm` shows references symbols; a subset of `dsymutil -s` output.
* `otool -l` all load commands in a mach-o file.
* `dwarfdump --file-stats lssym`
* `atos`
* `dwarfdump --lookup`
* run programs with `DYLD_PRINT_STATISTICS=1` set (see `man dyld` for more flags)

http://wiki.dwarfstd.org/index.php?title=Apple's_%22Lazy%22_DWARF_Scheme

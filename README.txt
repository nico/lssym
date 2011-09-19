* `dwarfdump` doesn't list `nlist` symbols
* `dsymutil` only works with linked debug info (hm, `dsymutil -s`)?
* `nm` is pretty cool but has no symbol sizes
* nlists are pretty hidden
* `otool -l` shows LC_DYLD_INFO_ONLY,nsyms for more than nm has output (maybe?)
* Also `otool -l` LC_DYSYMTAB
* Not clear if `nm` prints nlist or dwarf or what

http://wiki.dwarfstd.org/index.php?title=Apple's_%22Lazy%22_DWARF_Scheme

Also, I'd like to learn about this.

Other tools that are somewhat similar:
* `atos`
* `dwarfdump`'s `--lookup`

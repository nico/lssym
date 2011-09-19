/*
clang -o lssym lssym.c
*/
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

static void fatal(const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  vfprintf(stderr, msg, args);
  va_end(args);
  exit(1);
}

static void dump_symtab(struct symtab_command* symtab, uint8_t* data) {
  struct nlist_64* nlist = (struct nlist_64*)(data + symtab->symoff);

  char* strtab = (char*)(data + symtab->stroff);

  for (int i = 0; i < symtab->nsyms; ++i, nlist++) {
    printf("%30s", nlist->n_un.n_strx ? strtab + nlist->n_un.n_strx : "(null)");

    printf("  ");
    if (nlist->n_type & N_STAB)
      printf("  N_STAB %x", nlist->n_type);
    else {
      printf("%6s", nlist->n_type & N_PEXT ? "N_PEXT" : "");
      printf("%6s", nlist->n_type & N_EXT ? "N_EXT" : "");
      printf(" type ");
      switch (nlist->n_type & N_TYPE) {
        case N_UNDF: printf("N_UNDF"); break;
        case N_ABS:  printf("N_ABS "); break;
        case N_SECT: printf("N_SECT"); break;
        case N_PBUD: printf("N_PBUD"); break;
        case N_INDR: printf("N_INDR -> %s", strtab + nlist->n_value); break;
        default: printf("%x", nlist->n_type & N_TYPE); break;
      }
    }

    printf("    n_sect %03d", nlist->n_sect);
    printf(" n_desc 0x%04x", nlist->n_desc);
    printf("    n_value 0x%llx", nlist->n_value);
    printf("\n");
  }
}

static void dump_load_dylinker(struct dylinker_command* cmd) {
  printf("  Name: %s\n", ((char*)cmd) + cmd->name.offset);
}

static void parse_version(
    uint32_t version, unsigned* major, unsigned* minor, unsigned* dot) {
  *major = version >> 16;
  *minor = (version >> 8) & 0xff;
  *dot = version & 0xff;
}

static void dump_load_dylib(struct dylib_command* cmd) {
  struct dylib* dylib = &cmd->dylib;

  unsigned cur_ver, cur_maj, cur_dot;
  parse_version(dylib->current_version, &cur_ver, &cur_maj, &cur_dot);

  unsigned comp_ver, comp_maj, comp_dot;
  parse_version(dylib->compatibility_version, &comp_ver, &comp_maj, &comp_dot);

  printf("  Name: %s", ((char*)cmd) + dylib->name.offset);
  printf("  Timestamp: 0x%x  Version: %u.%u.%u  Compat version: %u.%u.%u\n",
      dylib->timestamp,
      cur_ver, cur_maj, cur_dot,
      comp_ver, comp_maj, comp_dot);
}

static void dump(struct mach_header* header) {
  uint8_t* data = (uint8_t*)header;

  if (header->magic == MH_CIGAM || header->magic == MH_CIGAM_64)
    fatal("non-host-ending binaries not supported\n");

  if (header->magic == FAT_CIGAM || header->magic == FAT_MAGIC)
    fatal("fat binaries not yet supported\n");

  if (header->magic == MH_MAGIC)
    fatal("only 64bit binaries supported atm\n");

  if (header->magic != MH_MAGIC_64)
    fatal("unexpected magic %x\n", header->magic);

  printf("Filetype: ");
  switch(header->filetype) {
    case MH_OBJECT: printf("MH_OBJECT"); break;
    case MH_EXECUTE: printf("MH_EXECUTE"); break;
    case MH_FVMLIB: printf("MH_FVMLIB"); break;
    case MH_CORE: printf("MH_CORE"); break;
    case MH_PRELOAD: printf("MH_PRELOAD"); break;
    case MH_DYLIB: printf("MH_DYLIB"); break;
    case MH_DYLINKER: printf("MH_DYLINKER"); break;
    case MH_BUNDLE: printf("MH_BUNDLE"); break;
    case MH_DYLIB_STUB: printf("MH_DYLIB_STUB"); break;
    case MH_DSYM: printf("MH_DSYM"); break;
    case MH_KEXT_BUNDLE: printf("MH_KEXT_BUNDLE"); break;
    default: printf("unknown 0x%x", header->filetype);
  }
  printf("\n");

  data += sizeof(struct mach_header_64);
  printf("%d load commands\n", header->ncmds);
  for (int i = 0; i < header->ncmds; ++i) {
    struct load_command* cmd = (struct load_command*)data;
    const char* cmdnam = "Unknown";
    switch (cmd->cmd & ~LC_REQ_DYLD) {
      case LC_SEGMENT_64: cmdnam = "LC_SEGMENT_64"; break;
      case LC_DYLD_INFO: cmdnam = "LC_DYLD_INFO"; break;
      case LC_SYMTAB: cmdnam = "LC_SYMTAB"; break;
      case LC_DYSYMTAB: cmdnam = "LC_DYSYMTAB"; break;
      case LC_LOAD_DYLINKER: cmdnam = "LC_LOAD_DYLINKER"; break;
      case LC_UUID: cmdnam = "LC_UUID"; break;
      case LC_UNIXTHREAD: cmdnam = "LC_UNIXTHREAD"; break;

      case LC_LOAD_DYLIB: cmdnam = "LC_LOAD_DYLIB"; break;
      case LC_LOAD_WEAK_DYLIB: cmdnam = "LC_LOAD_WEAK_DYLIB"; break;
      case LC_REEXPORT_DYLIB: cmdnam = "LC_REEXPORT_DYLIB"; break;
      case LC_LOAD_UPWARD_DYLIB: cmdnam = "LC_LOAD_UPWARD_DYLIB"; break;
      case LC_LAZY_LOAD_DYLIB: cmdnam = "LC_LAZY_LOAD_DYLIB"; break;
    }
    printf("cmd %s (0x%x), size %d\n", cmdnam, cmd->cmd, cmd->cmdsize);

    switch(cmd->cmd) {
      case LC_SYMTAB:
        dump_symtab((struct symtab_command*)cmd, (uint8_t*)header);
        break;
      case LC_LOAD_DYLINKER:
        dump_load_dylinker((struct dylinker_command*)cmd);
        break;
      case LC_LOAD_DYLIB:
      case LC_LOAD_WEAK_DYLIB:
      case LC_REEXPORT_DYLIB:
      case LC_LOAD_UPWARD_DYLIB:
      case LC_LAZY_LOAD_DYLIB:
        dump_load_dylib((struct dylib_command*)cmd);
        break;
    }

    data += cmd->cmdsize;
  }
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    return 1;
  }
  const char* in_name = argv[1];

  // Read input.
  int in_file = open(in_name, O_RDONLY);
  if (!in_file)
    fatal("Unable to read \'%s\'\n", in_name);

  struct stat in_stat;
  if (fstat(in_file, &in_stat))
    fatal("Failed to stat \'%s\'\n", in_name);

  // Casting memory like this is not portable.
  struct mach_header* header = (struct mach_header*)mmap(
      /*addr=*/0, in_stat.st_size,
      PROT_READ, MAP_SHARED, in_file, /*offset=*/0);
  if (header == MAP_FAILED)
    fatal("Failed to mmap: %d (%s)\n", errno, strerror(errno));

  dump(header);

  munmap(header, in_stat.st_size);
  close(in_file);
}

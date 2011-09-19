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
    printf("cmd 0x%x, size %d\n", cmd->cmd, cmd->cmdsize);

    switch(cmd->cmd) {
      case LC_SYMTAB:
        dump_symtab((struct symtab_command*)cmd, (uint8_t*)header);
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

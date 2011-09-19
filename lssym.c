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
    printf("%s 0x%x %d 0x%x 0x%llx\n",
        nlist->n_un.n_strx ? strtab + nlist->n_un.n_strx : "(empty)",
        nlist->n_type,
        nlist->n_sect,
        nlist->n_desc,
        nlist->n_value);
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

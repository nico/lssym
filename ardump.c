/*
clang -o ardump ardump.c
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

#include <ar.h>
#include <mach-o/fat.h>  // FAT_MAGIC

static void fatal(const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  vfprintf(stderr, msg, args);
  va_end(args);
  exit(1);
}

static void printn(const char* str, int n) {
  for (int i = 0; i < n && str[i]; ++i)
    printf("%c", str[i]);
}

static void mprintn(const char* m, const char* str, int n) {
  printf("%s", m);
  for (int i = 0; i < n && str[i]; ++i)
    printf("%c", str[i]);
  printf("\n");
}

void dump(void* contents) {
  if (strncmp(contents, ARMAG, SARMAG)) {
    uint32_t v = *(uint32_t*)contents;
    if (v == FAT_MAGIC || v == FAT_CIGAM)
      fatal("Fat archives are not supported by this tool.\n");
    fatal("File does not start with '%s'.\n", ARMAG);
  }

  struct ar_hdr* header = (struct ar_hdr*)(contents + SARMAG);
  void* rest = contents + SARMAG + sizeof(*header);

  printf("ar_name: ");
  if (strncmp(header->ar_name, AR_EFMT1, sizeof(AR_EFMT1) - 1)) {
    printn(header->ar_name, sizeof(header->ar_name));
    printf("\n");
  } else {
    int len = atoi(header->ar_name + sizeof(AR_EFMT1) - 1);
    printn(rest, len);
    rest += len;
    printf(" (extended BSD name)\n");
  }

  mprintn("ar_date: ", header->ar_date, sizeof(header->ar_date));
  mprintn("ar_uid: ", header->ar_uid, sizeof(header->ar_uid));
  mprintn("ar_gid: ", header->ar_gid, sizeof(header->ar_gid));
  mprintn("ar_mode: ", header->ar_mode, sizeof(header->ar_mode));
  mprintn("ar_size: ", header->ar_size, sizeof(header->ar_size));
  mprintn("ar_fmag: ", header->ar_fmag, sizeof(header->ar_fmag));
  if (strncmp(header->ar_fmag, ARFMAG, sizeof(ARFMAG) - 1))
    fatal("unexpected ar_fmag\n");
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
  void* ar_contents = mmap(
      /*addr=*/0, in_stat.st_size,
      PROT_READ, MAP_SHARED, in_file, /*offset=*/0);
  if (ar_contents == MAP_FAILED)
    fatal("Failed to mmap: %d (%s)\n", errno, strerror(errno));

  dump(ar_contents);

  munmap(ar_contents, in_stat.st_size);
  close(in_file);
}
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
#include <mach-o/ranlib.h>

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

// Returns if the name is a bsd extended name.
static int arobj_name(struct ar_hdr* header, char** out_name, size_t* out_len) {
  if (strncmp(header->ar_name, AR_EFMT1, sizeof(AR_EFMT1) - 1)) {
    *out_name = header->ar_name;
    *out_len = strnlen(header->ar_name, sizeof(header->ar_name));
    return 0;
  }
  *out_name = (char*)(header + 1);
  *out_len = atoi(header->ar_name + sizeof(AR_EFMT1) - 1);
  return 1;
}

static void dump_obj(struct ar_hdr* header) {
  char* name;
  size_t name_len;
  int is_bsd_name = arobj_name(header, &name, &name_len);

  printf("ar_name: ");
  printn(name, name_len);
  if (is_bsd_name) {
    printf(" (extended BSD name)");
  }
  printf("\n");

  mprintn("ar_date: ", header->ar_date, sizeof(header->ar_date));
  mprintn("ar_uid: ", header->ar_uid, sizeof(header->ar_uid));
  mprintn("ar_gid: ", header->ar_gid, sizeof(header->ar_gid));
  mprintn("ar_mode: ", header->ar_mode, sizeof(header->ar_mode));
  mprintn("ar_size: ", header->ar_size, sizeof(header->ar_size));
  mprintn("ar_fmag: ", header->ar_fmag, sizeof(header->ar_fmag));
  if (strncmp(header->ar_fmag, ARFMAG, sizeof(ARFMAG) - 1))
    fatal("unexpected ar_fmag\n");
}

static long long arobj_size(struct ar_hdr* header) {
  char buf[sizeof(header->ar_size) + 1];
  memcpy(buf, header->ar_size, sizeof(header->ar_size));
  buf[sizeof(header->ar_size)] = '\0';
  return strtoll(buf, NULL, 10) + sizeof(*header);
}

static void dump_symdefs(struct ar_hdr* header, size_t offset) {
  void* data = (char*)(header + 1) + offset;

  uint32_t ranlib_len = *(uint32_t*)data;
  uint32_t nranlibs = ranlib_len / sizeof(struct ranlib);
  struct ranlib* ranlib = (struct ranlib*)(data + sizeof(uint32_t));

  printf("%d ranlibs\n", nranlibs);

  data += ranlib_len + sizeof(uint32_t);
  uint32_t strtab_len = *(uint32_t*)data;
  char* strtab = (char*)(data + sizeof(uint32_t));

  for (int i = 0; i < nranlibs; ++i, ++ranlib) {
    printf("ran_strx 0x%x: %s, ran_off 0x%x\n",
           ranlib->ran_un.ran_strx,
           strtab + ranlib->ran_un.ran_strx,
           ranlib->ran_off);
  }
}

static void dump(void* contents, void* contents_end) {
  if (strncmp(contents, ARMAG, SARMAG)) {
    uint32_t v = *(uint32_t*)contents;
    if (v == FAT_MAGIC || v == FAT_CIGAM)
      fatal("Fat archives are not supported by this tool.\n");
    fatal("File does not start with '%s'.\n", ARMAG);
  }
  contents += SARMAG;

  while (contents < contents_end) {
    struct ar_hdr* header = (struct ar_hdr*)(contents);
    dump_obj(header);

    char* name;
    size_t name_len;
    int is_bsd_name = arobj_name(header, &name, &name_len);
    if(!strncmp(name, SYMDEF, name_len) ||
       !strncmp(name, SYMDEF_SORTED, name_len)) {
      dump_symdefs(header, is_bsd_name ? name_len : 0);
    }

    contents += arobj_size(header);
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
  void* ar_contents = mmap(
      /*addr=*/0, in_stat.st_size,
      PROT_READ, MAP_SHARED, in_file, /*offset=*/0);
  if (ar_contents == MAP_FAILED)
    fatal("Failed to mmap: %d (%s)\n", errno, strerror(errno));

  dump(ar_contents, ar_contents + in_stat.st_size);

  munmap(ar_contents, in_stat.st_size);
  close(in_file);
}

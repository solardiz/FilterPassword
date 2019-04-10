#include "hexutil.h"
#include "mappeablexorfilter.h"
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <iostream>
#include <limits.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

static void printusage(char *command) {
  printf(" Try %s -f xor8  filter.bin  7C4A8D09CA3762AF6 \n", command);
  ;
}

int main(int argc, char **argv) {
  int c;
  while ((c = getopt(argc, argv, "h")) != -1)
    switch (c) {
    case 'h':
      printusage(argv[0]);
      return 0;
    default:
      abort();
    }
  if (optind + 1 >= argc) {
    printusage(argv[0]);
    return -1;
  }
  const char *filename = argv[optind];
  const char *hashhex = argv[optind + 1];
  if (strlen(hashhex) < 16) {
    printf("bad hex. length is %zu \n", strlen(hashhex));
    printusage(argv[0]);
    return -1;
  }
  uint64_t x1 = hex_to_u32_nocheck((const uint8_t *)hashhex);
  uint64_t x2 = hex_to_u32_nocheck((const uint8_t *)hashhex + 4);
  uint64_t x3 = hex_to_u32_nocheck((const uint8_t *)hashhex + 8);
  uint64_t x4 = hex_to_u32_nocheck((const uint8_t *)hashhex + 12);
  if ((x1 | x2 | x3 | x4) > 0xFFFF) {
    printf("bad hex value  %s \n", hashhex);
    return EXIT_FAILURE;
  }
  uint64_t hexval = (x1 << 48) | (x2 << 32) | (x3 << 16) | x4;
  printf("hexval = 0x%" PRIx64 "\n", hexval);
  uint64_t cookie = 0;
  uint64_t expectedcookie = 1234567;

  uint64_t seed = 0;
  uint64_t BlockLength = 0;
  clock_t start = clock();

  // could open the file once (map it), instead of this complicated mess.
  FILE *fp = fopen(filename, "rb");
  if (fp == NULL) {
    printf("Cannot read the input file %s.", filename);
    return EXIT_FAILURE;
  }

  if(fread(&cookie, sizeof(cookie), 1, fp) != 1) printf("failed read.\n");
  if(fread(&seed, sizeof(seed), 1, fp) != 1) printf("failed read.\n");
  if(fread(&BlockLength, sizeof(BlockLength), 1, fp) != 1) printf("failed read.\n");
  if (cookie != expectedcookie) {
    printf("Not a filter file.\n");
    return EXIT_FAILURE;
  }
  fclose(fp);
  int fd = open(filename, O_RDONLY);
  bool shared = false;
  size_t length = 3 * BlockLength * sizeof(uint8_t) + 3 * sizeof(uint64_t);
  printf("I expect the file to span %zu bytes.\n", length);
  // for Linux:
#if defined(__linux__) && defined(USE_POPULATE)
  uint8_t *addr = (uint8_t *)(mmap(
      NULL, length, PROT_READ,
      MAP_FILE | (shared ? MAP_SHARED : MAP_PRIVATE) | MAP_POPULATE, fd, 0));
#else
  uint8_t *addr =
      (uint8_t *)(mmap(NULL, length, PROT_READ,
                       MAP_FILE | (shared ? MAP_SHARED : MAP_PRIVATE), fd, 0));
#endif
  if (addr == MAP_FAILED) {
    printf("Data can't be mapped???\n");
    return EXIT_FAILURE;
  } else {
    printf("memory mapping is a success.\n");
  }
  printf("Mapping the filter...\n");
  MappeableXorFilter<uint8_t> filter(BlockLength, seed,
                                     addr + 3 * sizeof(uint64_t));
  printf("Success!\n");
  if (filter.Contain(hexval)) {
    printf("Probably in the set.\n");
  } else {
    printf("Surely not in the set.\n");
  }
  clock_t end = clock();

  printf("Processing time %.3f seconds.\n",
         (float)(end - start) / CLOCKS_PER_SEC);

  munmap(addr, length);
  return EXIT_SUCCESS;
}
#include "ufdt_overlay.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "libufdt_sysdeps.h"


char *load_file(char *fname, size_t *pLen);

char *load_file(char *fname, size_t *pLen) {
  FILE *f;
  f = fopen(fname, "r");
  if (!f) {
    printf("Couldn't open file '%s'\n", fname);
    exit(1);
  }
  fseek(f, 0, SEEK_END);
  *pLen = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buf = dto_malloc(*pLen);
  if (fread(buf, *pLen, 1, f) != 1) {
    printf("Bad fread");
    exit(1);
  }
  return buf;
}

int main(int argc, char **argv) {
  char *base_buf, *overlay_buf;
  FILE *out_file;
  struct fdt_header *blob;
  if (argc < 4) {
    printf("Usage: ov_test base_file overlay_file out_file\n");
    exit(1);
  }
  size_t blob_len, overlay_len;
  base_buf = load_file(argv[1], &blob_len);
  overlay_buf = load_file(argv[2], &overlay_len);
  if (!overlay_buf) return 1;
  blob = ufdt_install_blob(base_buf, blob_len);

  if (!blob) {
    printf("ufdt_install_blob returned null\n");
    exit(1);
  }
  clock_t start, end;
  double cpu_time_used;
  start = clock();
  struct fdt_header *new_blob =
      ufdt_apply_overlay(blob, blob_len, overlay_buf, overlay_len);
  end = clock();
  cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;

  printf("ufdt_apply_overlay took %.9f second\n", cpu_time_used);

  // Do not dto_free(blob) - it's the same as base_buf.

  out_file = fopen(argv[3], "wb");
  if (fwrite(new_blob, 1, fdt_totalsize(new_blob), out_file) < 1) {
    printf("fwrite failed\n");
    exit(1);
  }
  free(new_blob);
  free(base_buf);
  free(overlay_buf);
  return 0;
}

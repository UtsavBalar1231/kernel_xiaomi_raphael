#include "util.h"

#include <stdio.h>
#include <stdlib.h>

#include "libfdt.h"
#include "libufdt_sysdeps.h"


char *load_file(const char *filename, size_t *pLen) {
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    return NULL;
  }

  // Gets the file size.
  fseek(fp, 0, SEEK_END);
  size_t len = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  char *buf = dto_malloc(len);
  if (fread(buf, len, 1, fp) != 1) {
    dto_free(buf);
    return NULL;
  }

  if (pLen) {
    *pLen = len;
  }
  return buf;
}

int write_buf_to_file(const char *filename,
                      const void *buf, size_t buf_size) {
  int ret = 0;
  FILE *fout = NULL;

  fout = fopen(filename, "wb");
  if (!fout) {
    ret = 1;
    goto end;
  }
  if (fwrite(buf, 1, buf_size, fout) < 1) {
    ret = 2;
    goto end;
  }

end:
  if (fout) fclose(fout);

  return ret;
}

int write_fdt_to_file(const char *filename, const void *fdt) {
  return write_buf_to_file(filename, fdt, fdt_totalsize(fdt));
}

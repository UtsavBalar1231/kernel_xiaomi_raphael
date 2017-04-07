#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>

char *load_file(const char *filename, size_t *len_ptr);
int write_buf_to_file(const char *filename, const void *buf, size_t buf_size);
int write_fdt_to_file(const char *filename, const void *fdt);

#endif

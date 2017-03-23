#ifndef MKDTIMG_CORE_H
#define MKDTIMG_CORE_H

#include <stdint.h>
#include <stdio.h>

#define OPTION_VALUE_SIZE_MAX 512


struct dt_image_writer;

int parse_option(char **option_ptr, char **value_ptr, char *line_str);
int parse_path(char **path_ptr, char **prop_ptr, char *value_str);

struct dt_image_writer *dt_image_writer_start(FILE *img_fp, uint32_t entry_count);
int set_global_options(struct dt_image_writer *writer, const char *option, const char *value);
int set_entry_options(struct dt_image_writer *writer, const char *option, const char *value);
int dt_image_writer_add_entry(struct dt_image_writer *writter, const char *fdt_filename);
int dt_image_writer_end(struct dt_image_writer *writter);

#endif

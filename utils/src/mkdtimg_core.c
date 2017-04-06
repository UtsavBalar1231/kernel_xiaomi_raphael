#include "mkdtimg_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "libfdt.h"

#include "dt_table.h"

#define DEBUG 0


struct dt_options {
  char id[OPTION_VALUE_SIZE_MAX];
  char rev[OPTION_VALUE_SIZE_MAX];
  char custom[4][OPTION_VALUE_SIZE_MAX];
};

struct dt_global_options {
  struct dt_options default_options;
  uint32_t page_size;
};

struct dt_image_writer {
  FILE *img_fp;

  struct dt_global_options global_options;
  struct dt_options entry_options;

  char entry_filename[1024];
  uint32_t entry_count;
  uint32_t entry_offset;
  uint32_t dt_offset;

  char (*past_filenames)[1024];
  uint32_t *past_dt_offsets;
};


static void init_dt_options(struct dt_options *options) {
  memset(options, 0, sizeof(struct dt_options));
}

static void init_dt_global_options(struct dt_global_options *options) {
  init_dt_options(&options->default_options);
  options->page_size = DT_TABLE_DEFAULT_PAGE_SIZE;
}

static void copy_dt_options(struct dt_options *target, struct dt_options *options) {
  memcpy(target, options, sizeof(struct dt_options));
}

static char *load_file_contents(FILE *fp, size_t *len_ptr) {
  // Gets the file size.
  fseek(fp, 0, SEEK_END);
  size_t len = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  char *buf = malloc(len);
  if (buf == NULL) {
    return NULL;
  }

  if (fread(buf, len, 1, fp) != 1) {
    free(buf);
    return NULL;
  }

  if (len_ptr) {
    *len_ptr = len;
  }

  return buf;
}

static char *load_file(const char *filename, size_t *len_ptr) {
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    return NULL;
  }

  char *buf = load_file_contents(fp, len_ptr);

  fclose(fp);

  return buf;
}

static int split_str(char **lhs_ptr, char **rhs_ptr, char *string, char c) {
  char *middle_ptr = strchr(string, c);
  if (middle_ptr == NULL) {
    return -1;
  }

  *middle_ptr = '\0';

  *lhs_ptr = string;
  *rhs_ptr = middle_ptr + 1;

  return 0;
}

int parse_option(char **option_ptr, char **value_ptr, char *line_str) {
  return split_str(option_ptr, value_ptr, line_str, '=');
}

int parse_path(char **path_ptr, char **prop_ptr, char *value_str) {
  return split_str(path_ptr, prop_ptr, value_str, ':');
}

static fdt32_t get_fdt32_from_prop(void *fdt, const char *path, const char *prop) {
  int node_off = fdt_path_offset(fdt, path);
  if (node_off < 0) {
    fprintf(stderr, "Can not find node: %s\n", path);
    return 0;
  }

  int len;
  fdt32_t *prop_value_ptr = (fdt32_t *)fdt_getprop(fdt, node_off, prop, &len);
  if (prop_value_ptr == NULL) {
    fprintf(stderr, "Can not find property: %s:%s\n", path, prop);
    return 0;
  }

  fdt32_t value = fdt32_to_cpu(*prop_value_ptr);
  /* TODO: check len */
  if (DEBUG) printf("%s:%s => %08x\n", path, prop, fdt32_to_cpu(value));

  return value;
}

static fdt32_t get_fdt32_from_number_or_prop(void *fdt, char *value_str) {
  if (value_str[0] == '/') {
    char *path, *prop;
    if (parse_path(&path, &prop, value_str) != 0) {
      fprintf(stderr, "Wrong syntax: %s\n", value_str);
      return 0;
    }
    return get_fdt32_from_prop(fdt, path, prop);
  }

  /* It should be a number */
  char *end;
  uint32_t value = strtoul(value_str, &end, 0);
  /* TODO: check end */
  return cpu_to_fdt32(value);
}

static int output_img_header(FILE *img_fp,
                             uint32_t entry_count, uint32_t total_size,
                             struct dt_global_options *options) {
  struct dt_table_header header;
  dt_table_header_init(&header);
  header.dt_entry_count = cpu_to_fdt32(entry_count);
  header.total_size = cpu_to_fdt32(total_size);
  header.page_size = cpu_to_fdt32(options->page_size);

  fseek(img_fp, 0, SEEK_SET);
  fwrite(&header, sizeof(header), 1, img_fp);

  return 0;
}

static int32_t output_img_entry(FILE *img_fp, size_t entry_offset,
                                size_t dt_offset, const char *fdt_filename,
                                struct dt_options *options, int reuse_fdt) {
  int32_t ret = -1;
  void *fdt = NULL;

  size_t fdt_file_size;
  fdt = load_file(fdt_filename, &fdt_file_size);
  if (fdt == NULL) {
    fprintf(stderr, "Can not read file: %s\n", fdt_filename);
    goto end;
  }

  if (fdt_check_header(fdt) != 0) {
    fprintf(stderr, "Bad FDT header: \n", fdt_filename);
    goto end;
  }

  size_t fdt_size = fdt_totalsize(fdt);
  if (fdt_size != fdt_file_size) {
    fprintf(stderr, "The file size and FDT size are not matched: %s\n", fdt_filename);
    goto end;
  }

  /* Prepare dt_table_entry and output */
  struct dt_table_entry entry;
  entry.dt_size = cpu_to_fdt32(fdt_size);
  entry.dt_offset = cpu_to_fdt32(dt_offset);
  entry.id = get_fdt32_from_number_or_prop(fdt, options->id);
  entry.rev = get_fdt32_from_number_or_prop(fdt, options->rev);
  entry.custom[0] = get_fdt32_from_number_or_prop(fdt, options->custom[0]);
  entry.custom[1] = get_fdt32_from_number_or_prop(fdt, options->custom[1]);
  entry.custom[2] = get_fdt32_from_number_or_prop(fdt, options->custom[2]);
  entry.custom[3] = get_fdt32_from_number_or_prop(fdt, options->custom[3]);
  fseek(img_fp, entry_offset, SEEK_SET);
  fwrite(&entry, sizeof(entry), 1, img_fp);

  /* Output FDT */
  if (!reuse_fdt) {
    fseek(img_fp, dt_offset, SEEK_SET);
    fwrite(fdt, fdt_file_size, 1, img_fp);
    ret = fdt_file_size;
  } else {
    ret = 0;
  }

end:
  if (fdt) free(fdt);

  return ret;
}


struct dt_image_writer *dt_image_writer_start(FILE *img_fp, uint32_t entry_count) {
  struct dt_image_writer *writer = malloc(sizeof(struct dt_image_writer));
  if (!writer) goto error;
  writer->past_filenames = calloc(entry_count, sizeof(*writer->past_filenames));
  if (!writer->past_filenames) goto error;
  writer->past_dt_offsets =
      calloc(entry_count, sizeof(*writer->past_dt_offsets));
  if (!writer->past_dt_offsets) goto error;
  writer->img_fp = img_fp;
  init_dt_global_options(&writer->global_options);
  init_dt_options(&writer->entry_options);
  writer->entry_filename[0] = '\0';
  writer->entry_count = entry_count;
  writer->entry_offset = sizeof(struct dt_table_header);
  writer->dt_offset =
      writer->entry_offset + sizeof(struct dt_table_entry) * entry_count;
  return writer;

error:
  fprintf(stderr, "Unable to start writer\n");
  if (!writer) return NULL;
  if (writer->past_filenames) free(writer->past_filenames);
  if (writer->past_dt_offsets) free(writer->past_dt_offsets);
  free(writer);
  return NULL;
}

static int set_dt_options(struct dt_options *options,
                          const char *option, const char *value) {
  if (strcmp(option, "id") == 0) {
    strncpy(options->id, value, OPTION_VALUE_SIZE_MAX - 1);
  } else if (strcmp(option, "rev") == 0) {
    strncpy(options->rev, value, OPTION_VALUE_SIZE_MAX - 1);
  } else if (strcmp(option, "custom0") == 0) {
    strncpy(options->custom[0], value, OPTION_VALUE_SIZE_MAX - 1);
  } else if (strcmp(option, "custom1") == 0) {
    strncpy(options->custom[1], value, OPTION_VALUE_SIZE_MAX - 1);
  } else if (strcmp(option, "custom2") == 0) {
    strncpy(options->custom[2], value, OPTION_VALUE_SIZE_MAX - 1);
  } else if (strcmp(option, "custom3") == 0) {
    strncpy(options->custom[3], value, OPTION_VALUE_SIZE_MAX - 1);
  } else {
    return -1;
  }

  return 0;
}

int set_global_options(struct dt_image_writer *writer,
                       const char *option, const char *value) {
  struct dt_global_options *global_options = &writer->global_options;

  if (strcmp(option, "page_size") == 0) {
    global_options->page_size = strtoul(value, NULL, 0);
  } else {
    return set_dt_options(&global_options->default_options, option, value);
  }

  return 0;
}

int set_entry_options(struct dt_image_writer *writer,
                      const char *option, const char *value) {
  return set_dt_options(&writer->entry_options, option, value);
}

static int flush_entry_to_img(struct dt_image_writer *writer) {
  if (writer->entry_filename[0] == '\0') {
    return 0;
  }

  int reuse_fdt;
  int fdt_idx;
  uint32_t dt_offset;

  for (fdt_idx = 0; writer->past_filenames[fdt_idx][0] != '\0'; fdt_idx++) {
    if (strcmp(writer->past_filenames[fdt_idx], writer->entry_filename) == 0)
      break;
  }

  if (writer->past_filenames[fdt_idx][0] != '\0') {
    reuse_fdt = 1;
    dt_offset = writer->past_dt_offsets[fdt_idx];
  } else {
    reuse_fdt = 0;
    dt_offset = writer->dt_offset;
  }
  int32_t dt_size = output_img_entry(writer->img_fp, writer->entry_offset,
                                     dt_offset, writer->entry_filename,
                                     &writer->entry_options, reuse_fdt);
  if (dt_size == -1) return -1;

  if (!reuse_fdt) {
    strncpy(writer->past_filenames[fdt_idx], writer->entry_filename,
            sizeof(writer->past_filenames[fdt_idx]) - 1);
    writer->past_dt_offsets[fdt_idx] = dt_offset;
  }

  writer->entry_offset += sizeof(struct dt_table_entry);
  writer->dt_offset += dt_size;

  return 0;
}

int dt_image_writer_add_entry(struct dt_image_writer *writer,
                              const char *fdt_filename) {
  if (flush_entry_to_img(writer) != 0) {
    return -1;
  }

  strncpy(
      writer->entry_filename,
      fdt_filename,
      sizeof(writer->entry_filename) - 1);

  /* Copy the default_options as default */
  copy_dt_options(
      &writer->entry_options,
      &writer->global_options.default_options);

  return 0;
}

int dt_image_writer_end(struct dt_image_writer *writer) {
  int ret = -1;

  if (flush_entry_to_img(writer) != 0) {
    goto end;
  }

  if (output_img_header(
      writer->img_fp,
      writer->entry_count,
      writer->dt_offset,
      &writer->global_options) != 0) {
    goto end;
  }

  printf("Total %d entries.\n", writer->entry_count);
  ret = 0;

end:
  free(writer->past_filenames);
  free(writer->past_dt_offsets);
  free(writer);

  return ret;
}

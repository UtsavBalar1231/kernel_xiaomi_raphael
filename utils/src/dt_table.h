#ifndef DT_TABLE_H
#define DT_TABLE_H

#include <stdint.h>

/*
 * For the image layout, refer README.md for the detail
 */

#define DT_TABLE_MAGIC 0xd7b7ab1e
#define DT_TABLE_DEFAULT_PAGE_SIZE 2048

struct dt_table_header {
  uint32_t magic;             /* DT_TABLE_MAGIC */
  uint32_t total_size;        /* includes dt_table_header + all dt_table_entry
                                 and all dtb/dtbo */
  uint32_t header_size;       /* sizeof(dt_table_header) */

  uint32_t dt_entry_size;     /* sizeof(dt_table_entry) */
  uint32_t dt_entry_count;    /* number of dt_table_entry */
  uint32_t dt_entries_offset; /* offset to the first dt_table_entry
                                 from head of dt_table_header.
                                 The value will be equal to header_size if
                                 no padding is appended */

  uint32_t page_size;         /* flash page size we assume */
  uint32_t reserved[1];       /* must be zero */
};

struct dt_table_entry {
  uint32_t dt_size;
  uint32_t dt_offset;         /* offset from head of dt_table_header */

  uint32_t id;                /* optional, must be zero if unused */
  uint32_t rev;               /* optional, must be zero if unused */
  uint32_t custom[4];         /* optional, must be zero if unused */
};

void dt_table_header_init(struct dt_table_header *header);

#endif

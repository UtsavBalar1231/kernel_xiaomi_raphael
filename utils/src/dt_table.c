#include "dt_table.h"

#include <memory.h>

#include "libfdt.h"
#include "libufdt_sysdeps.h"


void dt_table_header_init(struct dt_table_header *header) {
  const uint32_t header_size = sizeof(struct dt_table_header);
  const uint32_t entry_size = sizeof(struct dt_table_entry);

  dto_memset(header, 0, header_size);
  header->magic = cpu_to_fdt32(DT_TABLE_MAGIC);
  header->total_size = cpu_to_fdt32(header_size);
  header->header_size = cpu_to_fdt32(header_size);
  header->dt_entry_size = cpu_to_fdt32(entry_size);
  header->dt_entries_offset = cpu_to_fdt32(header_size);
  header->page_size = cpu_to_fdt32(DT_TABLE_DEFAULT_PAGE_SIZE);
}

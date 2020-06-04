#ifndef _UFSTW_FS_H_
#define _UFSTW_FS_H_

#if defined(CONFIG_UFSTW)
#define ufstw_fs	0x01

extern void bdev_set_turbo_write(struct block_device *bdev);
extern void bdev_clear_turbo_write(struct block_device *bdev);
#endif
#endif

/*
 * Many of the syscalls used in this file expect some of the arguments
 * to be __user pointers not __kernel pointers.  To limit the sparse
 * noise, turn off sparse checking for this file.
 */
#ifdef __CHECKER__
#undef __CHECKER__
#warning "Sparse checking disabled for this file"
#endif

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/ctype.h>
#include <linux/fd.h>
#include <linux/tty.h>
#include <linux/suspend.h>
#include <linux/root_dev.h>
#include <linux/security.h>
#include <linux/delay.h>
#include <linux/genhd.h>
#include <linux/mount.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/initrd.h>
#include <linux/async.h>
#include <linux/fs_struct.h>
#include <linux/slab.h>
#include <linux/ramfs.h>
#include <linux/shmem_fs.h>

#include <linux/nfs_fs.h>
#include <linux/nfs_fs_sb.h>
#include <linux/nfs_mount.h>
#include <soc/qcom/boot_stats.h>
#include <linux/dirent.h>

#include "do_mounts.h"

int __initdata rd_doload;	/* 1 = load RAM disk, 0 = don't load */

int root_mountflags = MS_RDONLY | MS_SILENT;
static char * __initdata root_device_name;
static char __initdata saved_root_name[64];
static int root_wait;
#ifdef CONFIG_EARLY_SERVICES
static char saved_modem_name[64];
static char saved_early_userspace[64];
static char init_prog[128] = "/early_services/init_early";
static char *init_prog_argv[2] = { init_prog, NULL };
static int es_status; /*1= es mount is success 0= es failed to run*/
#define EARLY_SERVICES_MOUNT_POINT "/early_services"
#endif
dev_t ROOT_DEV;

static int __init load_ramdisk(char *str)
{
	rd_doload = simple_strtol(str,NULL,0) & 3;
	return 1;
}
__setup("load_ramdisk=", load_ramdisk);

static int __init readonly(char *str)
{
	if (*str)
		return 0;
	root_mountflags |= MS_RDONLY;
	return 1;
}

static int __init readwrite(char *str)
{
	if (*str)
		return 0;
	root_mountflags &= ~MS_RDONLY;
	return 1;
}

__setup("ro", readonly);
__setup("rw", readwrite);

#ifdef CONFIG_BLOCK
struct uuidcmp {
	const char *uuid;
	int len;
};

/**
 * match_dev_by_uuid - callback for finding a partition using its uuid
 * @dev:	device passed in by the caller
 * @data:	opaque pointer to the desired struct uuidcmp to match
 *
 * Returns 1 if the device matches, and 0 otherwise.
 */
static int match_dev_by_uuid(struct device *dev, const void *data)
{
	const struct uuidcmp *cmp = data;
	struct hd_struct *part = dev_to_part(dev);

	if (!part->info)
		goto no_match;

	if (strncasecmp(cmp->uuid, part->info->uuid, cmp->len))
		goto no_match;

	return 1;
no_match:
	return 0;
}


/**
 * devt_from_partuuid - looks up the dev_t of a partition by its UUID
 * @uuid_str:	char array containing ascii UUID
 *
 * The function will return the first partition which contains a matching
 * UUID value in its partition_meta_info struct.  This does not search
 * by filesystem UUIDs.
 *
 * If @uuid_str is followed by a "/PARTNROFF=%d", then the number will be
 * extracted and used as an offset from the partition identified by the UUID.
 *
 * Returns the matching dev_t on success or 0 on failure.
 */
static dev_t devt_from_partuuid(const char *uuid_str)
{
	dev_t res = 0;
	struct uuidcmp cmp;
	struct device *dev = NULL;
	struct gendisk *disk;
	struct hd_struct *part;
	int offset = 0;
	bool clear_root_wait = false;
	char *slash;

	cmp.uuid = uuid_str;

	slash = strchr(uuid_str, '/');
	/* Check for optional partition number offset attributes. */
	if (slash) {
		char c = 0;
		/* Explicitly fail on poor PARTUUID syntax. */
		if (sscanf(slash + 1,
			   "PARTNROFF=%d%c", &offset, &c) != 1) {
			clear_root_wait = true;
			goto done;
		}
		cmp.len = slash - uuid_str;
	} else {
		cmp.len = strlen(uuid_str);
	}

	if (!cmp.len) {
		clear_root_wait = true;
		goto done;
	}

	dev = class_find_device(&block_class, NULL, &cmp,
				&match_dev_by_uuid);
	if (!dev)
		goto done;

	res = dev->devt;

	/* Attempt to find the partition by offset. */
	if (!offset)
		goto no_offset;

	res = 0;
	disk = part_to_disk(dev_to_part(dev));
	part = disk_get_part(disk, dev_to_part(dev)->partno + offset);
	if (part) {
		res = part_devt(part);
		put_device(part_to_dev(part));
	}

no_offset:
	put_device(dev);
done:
	if (clear_root_wait) {
		pr_err("VFS: PARTUUID= is invalid.\n"
		       "Expected PARTUUID=<valid-uuid-id>[/PARTNROFF=%%d]\n");
		if (root_wait)
			pr_err("Disabling rootwait; root= is invalid.\n");
		root_wait = 0;
	}
	return res;
}
#endif

/*
 *	Convert a name into device number.  We accept the following variants:
 *
 *	1) <hex_major><hex_minor> device number in hexadecimal represents itself
 *         no leading 0x, for example b302.
 *	2) /dev/nfs represents Root_NFS (0xff)
 *	3) /dev/<disk_name> represents the device number of disk
 *	4) /dev/<disk_name><decimal> represents the device number
 *         of partition - device number of disk plus the partition number
 *	5) /dev/<disk_name>p<decimal> - same as the above, that form is
 *	   used when disk name of partitioned disk ends on a digit.
 *	6) PARTUUID=00112233-4455-6677-8899-AABBCCDDEEFF representing the
 *	   unique id of a partition if the partition table provides it.
 *	   The UUID may be either an EFI/GPT UUID, or refer to an MSDOS
 *	   partition using the format SSSSSSSS-PP, where SSSSSSSS is a zero-
 *	   filled hex representation of the 32-bit "NT disk signature", and PP
 *	   is a zero-filled hex representation of the 1-based partition number.
 *	7) PARTUUID=<UUID>/PARTNROFF=<int> to select a partition in relation to
 *	   a partition with a known unique id.
 *	8) <major>:<minor> major and minor number of the device separated by
 *	   a colon.
 *
 *	If name doesn't have fall into the categories above, we return (0,0).
 *	block_class is used to check if something is a disk name. If the disk
 *	name contains slashes, the device name has them replaced with
 *	bangs.
 */

dev_t name_to_dev_t(const char *name)
{
	char s[32];
	char *p;
	dev_t res = 0;
	int part;

#ifdef CONFIG_BLOCK
	if (strncmp(name, "PARTUUID=", 9) == 0) {
		name += 9;
		res = devt_from_partuuid(name);
		if (!res)
			goto fail;
		goto done;
	}
#endif

	if (strncmp(name, "/dev/", 5) != 0) {
		unsigned maj, min, offset;
		char dummy;

		if ((sscanf(name, "%u:%u%c", &maj, &min, &dummy) == 2) ||
		    (sscanf(name, "%u:%u:%u:%c", &maj, &min, &offset, &dummy) == 3)) {
			res = MKDEV(maj, min);
			if (maj != MAJOR(res) || min != MINOR(res))
				goto fail;
		} else {
			res = new_decode_dev(simple_strtoul(name, &p, 16));
			if (*p)
				goto fail;
		}
		goto done;
	}

	name += 5;
	res = Root_NFS;
	if (strcmp(name, "nfs") == 0)
		goto done;
	res = Root_RAM0;
	if (strcmp(name, "ram") == 0)
		goto done;

	if (strlen(name) > 31)
		goto fail;
	strcpy(s, name);
	for (p = s; *p; p++)
		if (*p == '/')
			*p = '!';
	res = blk_lookup_devt(s, 0);
	if (res)
		goto done;

	/*
	 * try non-existent, but valid partition, which may only exist
	 * after revalidating the disk, like partitioned md devices
	 */
	while (p > s && isdigit(p[-1]))
		p--;
	if (p == s || !*p || *p == '0')
		goto fail;

	/* try disk name without <part number> */
	part = simple_strtoul(p, NULL, 10);
	*p = '\0';
	res = blk_lookup_devt(s, part);
	if (res)
		goto done;

	/* try disk name without p<part number> */
	if (p < s + 2 || !isdigit(p[-2]) || p[-1] != 'p')
		goto fail;
	p[-1] = '\0';
	res = blk_lookup_devt(s, part);
	if (res)
		goto done;

fail:
	return 0;
done:
	return res;
}
EXPORT_SYMBOL_GPL(name_to_dev_t);

static int __init root_dev_setup(char *line)
{
	strlcpy(saved_root_name, line, sizeof(saved_root_name));
	return 1;
}

__setup("root=", root_dev_setup);

#ifdef CONFIG_EARLY_SERVICES
static int __init modem_dev_setup(char *line)
{
	strlcpy(saved_modem_name, line, sizeof(saved_modem_name));
	return 1;
}

__setup("modem=", modem_dev_setup);
static int __init early_userspace_setup(char *line)
{
	strlcpy(saved_early_userspace, line, sizeof(saved_early_userspace));
	return 1;
}

__setup("early_userspace=", early_userspace_setup);
#endif
static int __init rootwait_setup(char *str)
{
	if (*str)
		return 0;
	root_wait = 1;
	return 1;
}

__setup("rootwait", rootwait_setup);

static char * __initdata root_mount_data;
static int __init root_data_setup(char *str)
{
	root_mount_data = str;
	return 1;
}

static char * __initdata root_fs_names;
static int __init fs_names_setup(char *str)
{
	root_fs_names = str;
	return 1;
}

static unsigned int __initdata root_delay;
static int __init root_delay_setup(char *str)
{
	root_delay = simple_strtoul(str, NULL, 0);
	return 1;
}

__setup("rootflags=", root_data_setup);
__setup("rootfstype=", fs_names_setup);
__setup("rootdelay=", root_delay_setup);

static void __init get_fs_names(char *page)
{
	char *s = page;

	if (root_fs_names) {
		strcpy(page, root_fs_names);
		while (*s++) {
			if (s[-1] == ',')
				s[-1] = '\0';
		}
	} else {
		int len = get_filesystem_list(page);
		char *p, *next;

		page[len] = '\0';
		for (p = page-1; p; p = next) {
			next = strchr(++p, '\n');
			if (*p++ != '\t')
				continue;
			while ((*s++ = *p++) != '\n')
				;
			s[-1] = '\0';
		}
	}
	*s = '\0';
}

#ifdef CONFIG_EARLY_SERVICES
static void get_fs_names_runtime(char *page)
{
	char *s = page;
	int len = get_filesystem_list_runtime(page);
	char *p, *next;

	page[len] = '\0';

	for (p = page-1; p; p = next) {
		next = strnchr(++p, len, '\n');
		if (*p++ != '\t')
			continue;
		while ((*s++ = *p++) != '\n')
			;
		s[-1] = '\0';
	}
	*s = '\0';
}
#endif
static int __init do_mount_root(char *name, char *fs, int flags, void *data)
{
	struct super_block *s;
	int err;

	place_marker("M - DRIVER F/S Init");

	err = sys_mount((char __user *)name, (char __user *)"/root",
			(char __user *)fs, (unsigned long)flags,
						(void __user *)data);
	if (err)
		return err;

	sys_chdir("/root");
	s = current->fs->pwd.dentry->d_sb;
	ROOT_DEV = s->s_dev;
	printk(KERN_INFO
	       "VFS: Mounted root (%s filesystem)%s on device %u:%u.\n",
	       s->s_type->name,
	       sb_rdonly(s) ? " readonly" : "",
	       MAJOR(ROOT_DEV), MINOR(ROOT_DEV));

	place_marker("M - DRIVER F/S Ready");

	return 0;
}
#ifdef CONFIG_EARLY_SERVICES
static int do_mount_part(char *name, char *fs, int flags,
				void *data, char *mnt_point)
{
	int err;

	err = sys_mount((char __user *)name, (char __user *)mnt_point,
			(char __user *)fs, (unsigned long)flags,
						(void __user *)data);
	if (err) {
		pr_err("Mount Partition [%s] failed[%d]\n", name, err);
		return err;
	}
	return 0;
}
#endif
void __init mount_block_root(char *name, int flags)
{
	struct page *page = alloc_page(GFP_KERNEL);
	char *fs_names = page_address(page);
	char *p;
#ifdef CONFIG_BLOCK
	char b[BDEVNAME_SIZE];
#else
	const char *b = name;
#endif

	get_fs_names(fs_names);
retry:
	for (p = fs_names; *p; p += strlen(p)+1) {
		int err = do_mount_root(name, p, flags, root_mount_data);
		switch (err) {
			case 0:
				goto out;
			case -EACCES:
			case -EINVAL:
				continue;
		}
	        /*
		 * Allow the user to distinguish between failed sys_open
		 * and bad superblock on root device.
		 * and give them a list of the available devices
		 */
#ifdef CONFIG_BLOCK
		__bdevname(ROOT_DEV, b);
#endif
		printk("VFS: Cannot open root device \"%s\" or %s: error %d\n",
				root_device_name, b, err);
		printk("Please append a correct \"root=\" boot option; here are the available partitions:\n");

		printk_all_partitions();
#ifdef CONFIG_DEBUG_BLOCK_EXT_DEVT
		printk("DEBUG_BLOCK_EXT_DEVT is enabled, you need to specify "
		       "explicit textual name for \"root=\" boot option.\n");
#endif
		panic("VFS: Unable to mount root fs on %s", b);
	}
	if (!(flags & SB_RDONLY)) {
		flags |= SB_RDONLY;
		goto retry;
	}

	printk("List of all partitions:\n");
	printk_all_partitions();
	printk("No filesystem could mount root, tried: ");
	for (p = fs_names; *p; p += strlen(p)+1)
		printk(" %s", p);
	printk("\n");
#ifdef CONFIG_BLOCK
	__bdevname(ROOT_DEV, b);
#endif
	panic("VFS: Unable to mount root fs on %s", b);
out:
	put_page(page);
}
 
#ifdef CONFIG_ROOT_NFS

#define NFSROOT_TIMEOUT_MIN	5
#define NFSROOT_TIMEOUT_MAX	30
#define NFSROOT_RETRY_MAX	5

static int __init mount_nfs_root(void)
{
	char *root_dev, *root_data;
	unsigned int timeout;
	int try, err;

	err = nfs_root_data(&root_dev, &root_data);
	if (err != 0)
		return 0;

	/*
	 * The server or network may not be ready, so try several
	 * times.  Stop after a few tries in case the client wants
	 * to fall back to other boot methods.
	 */
	timeout = NFSROOT_TIMEOUT_MIN;
	for (try = 1; ; try++) {
		err = do_mount_root(root_dev, "nfs",
					root_mountflags, root_data);
		if (err == 0)
			return 1;
		if (try > NFSROOT_RETRY_MAX)
			break;

		/* Wait, in case the server refused us immediately */
		ssleep(timeout);
		timeout <<= 1;
		if (timeout > NFSROOT_TIMEOUT_MAX)
			timeout = NFSROOT_TIMEOUT_MAX;
	}
	return 0;
}
#endif

#if defined(CONFIG_BLK_DEV_RAM) || defined(CONFIG_BLK_DEV_FD)
void __init change_floppy(char *fmt, ...)
{
	struct termios termios;
	char buf[80];
	char c;
	int fd;
	va_list args;
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
	fd = sys_open("/dev/root", O_RDWR | O_NDELAY, 0);
	if (fd >= 0) {
		sys_ioctl(fd, FDEJECT, 0);
		sys_close(fd);
	}
	printk(KERN_NOTICE "VFS: Insert %s and press ENTER\n", buf);
	fd = sys_open("/dev/console", O_RDWR, 0);
	if (fd >= 0) {
		sys_ioctl(fd, TCGETS, (long)&termios);
		termios.c_lflag &= ~ICANON;
		sys_ioctl(fd, TCSETSF, (long)&termios);
		sys_read(fd, &c, 1);
		termios.c_lflag |= ICANON;
		sys_ioctl(fd, TCSETSF, (long)&termios);
		sys_close(fd);
	}
}
#endif

void __init mount_root(void)
{
#ifdef CONFIG_ROOT_NFS
	if (ROOT_DEV == Root_NFS) {
		if (mount_nfs_root())
			return;

		printk(KERN_ERR "VFS: Unable to mount root fs via NFS, trying floppy.\n");
		ROOT_DEV = Root_FD0;
	}
#endif
#ifdef CONFIG_BLK_DEV_FD
	if (MAJOR(ROOT_DEV) == FLOPPY_MAJOR) {
		/* rd_doload is 2 for a dual initrd/ramload setup */
		if (rd_doload==2) {
			if (rd_load_disk(1)) {
				ROOT_DEV = Root_RAM1;
				root_device_name = NULL;
			}
		} else
			change_floppy("root floppy");
	}
#endif
#ifdef CONFIG_BLOCK
	{
		int err = create_dev("/dev/root", ROOT_DEV);

		if (err < 0)
			pr_emerg("Failed to create /dev/root: %d\n", err);
		mount_block_root("/dev/root", root_mountflags);
	}
#endif
}

#ifdef CONFIG_EARLY_SERVICES
int get_early_services_status(void)
{
	return es_status;
}

static int mount_partition(char *part_name, char *mnt_point)
{
	struct page *page = alloc_page(GFP_KERNEL);
	char *fs_names = page_address(page);
	char *p;
	int err = -EPERM;

	if (!part_name[0]) {
		pr_err("Unknown partition\n");
		return -ENOENT;
	}

	get_fs_names_runtime(fs_names);
	for (p = fs_names; *p; p += strlen(p)+1) {
		err = do_mount_part(part_name, p, root_mountflags,
					NULL, mnt_point);
		switch (err) {
		case 0:
			return err;
		case -EACCES:
		case -EINVAL:
			continue;
		}
		return err;
	}
	return err;
}
void launch_early_services(void)
{
	int rc = 0;

	devtmpfs_mount("dev");
	rc = mount_partition(saved_early_userspace, EARLY_SERVICES_MOUNT_POINT);
	if (!rc) {
		place_marker("Early Services Partition ready");
		rc = call_usermodehelper(init_prog, init_prog_argv, NULL, 0);
		if (!rc) {
			es_status = 1;
			pr_info("early_init launched\n");
		} else
			pr_err("early_init failed\n");
	}
}
#else
void launch_early_services(void) { }
#endif

#define PREFETCH_NUM 16
#define PREFETCH_BUF_LEN 8096

static char *systemd_files_1[] = {
	"f/lib/systemd/system/systemd-update-utmp.service",
	"f/lib/systemd/system/umount.target",
	"f/lib/systemd/system/systemd-journal-catalog-update.service",
	"f/lib/systemd/system/systemd-hwdb-update.service",
	"f/lib/systemd/system/systemd-ask-password-console.path",
	"f/lib/systemd/system/sys-kernel-config.mount",
	"f/lib/systemd/system/sys-fs-fuse-connections.mount",
	"f/lib/systemd/system/ip6tables.service",
	"f/lib/systemd/system/init_post_boot.service",
	"f/lib/systemd/system/adbd.service",
	"f/lib/systemd/system/acdb_loader.service",
	"f/lib/systemd/system/ab-updater.service",
	"f/lib/systemd/system/persist-prop.service",
	"f/etc/systemd/journald.conf",
	"f/lib/systemd/journald.conf.d/00-systemd-conf.conf",
	"d/etc/systemd/system/multi-user.target.wants",
	"d/etc/systemd/system/getty.target.wants",
	"d/etc/systemd/system/timers.target.wants",
	"d/lib/systemd/system/timers.target.wants",
	NULL
};

static char *systemd_files_2[] = {
	"f/lib/systemd/systemd",
	"f/etc/init.d/setup_avtp_routing_le",
	"f/lib/systemd/system/multi-user.target",
	"f/lib/systemd/system/local-fs-pre.target",
	"d/etc/systemd/system/local-fs-pre.target.wants",
	"f/lib/systemd/system/init_data.service",
	"f/lib/systemd/system/init_audio.service",
	"f/lib/systemd/system/rc-local.service",
	"f/lib/systemd/system/getty@.service",
	"f/lib/systemd/system/systemd-tmpfiles-clean.service",
	"f/lib/systemd/system/logrotate.timer",
	NULL
};

static char *systemd_files_3[] = {
	"f/lib/systemd/system.conf.d/00-systemd-conf.conf",
	"f/etc/fstab",
	"d/etc/rc1.d",
	"d/media/ram",
	"f/lib/systemd/system/time-sync.target",
	"f/lib/systemd/system/swap.target",
	"f/lib/systemd/system/remote-fs.target",
	"f/lib/systemd/system/systemd-tmpfiles-setup-dev.service",
	"f/lib/systemd/system/systemd-timesyncd.service",
	"f/lib/systemd/system/var-allplay.service",
	"f/lib/systemd/system/logrotate.service",
	"f/lib/systemd/system/kmod-static-nodes.service",
	"f/lib/systemd/system/init_early_boot.service",
	NULL
};

static char *systemd_files_4[] = {
	"f/etc/hostname",
	"f/lib/systemd/system-generators/systemd-sysv-generator",
	"f/lib/libpthread-2.31.so",
	"f/lib/libpthread.so.0",
	"f/lib/systemd/system/initrd-fs.target",
	"f/lib/systemd/system/initrd-root-fs.target",
	"f/lib/systemd/system/rescue.target",
	"f/lib/systemd/system/emergency.service",
	"f/lib/systemd/system/dev-mqueue.mount",
	"f/lib/systemd/system/dev-hugepages.mount",
	NULL
};

static char *systemd_files_5[] = {
	"f/lib/systemd/system/sysinit.target",
	"f/lib/systemd/system/weston.service",
	"f/lib/systemd/system/systemd-ask-password-console.service",
	"f/lib/systemd/system/systemd-vconsole-setup.service",
	"f/lib/systemd/system/initrd-switch-root.target",
	"f/lib/systemd/system/qrtr-ns.service",
	"f/lib/systemd/system/pdmapper.service",
	"f/lib/systemd/system/systemd-update-utmp-runlevel.service",
	"f/lib/systemd/system/graphical.target",
	"f/lib/systemd/system/amfs.service",
	"f/lib/systemd/system/ais_server.service",
	"f/etc/systemd/system/sfsconfig.service",
	"f/lib/systemd/system/timers.target",
	"f/lib/systemd/system/systemd-tmpfiles-clean.timer",
	"f/lib/systemd/system/systemd-journald.socket",
	"f/etc/default/rng-tools",
	NULL
};

static char *systemd_files_6[] = {
	"f/etc/machine-id",
	"f/var/lib/dbus/machine-id",
	"f/lib/systemd/system/shutdown.target",
	"f/lib/systemd/system/xinetd.service",
	"f/lib/librt-2.31.so",
	"f/lib/librt.so.1",
	"f/lib/systemd/system/var-lib-shared.mount",
	"f/lib/systemd/system/initrd-udevadm-cleanup-db.service",
	"f/lib/systemd/system/systemd-udev-settle.service",
	"f/lib/systemd/system/basic.target",
	"f/lib/systemd/system/setup-network.service",
	"f/lib/systemd/system/dbus.service",
	"f/lib/systemd/system/var-adb_devid.service",
	"f/lib/systemd/system/user.slice",
	"f/lib/systemd/system/systemd-ask-password-wall.path",
	"f/lib/systemd/system/strongswan-starter.service",
	"f/lib/systemd/system/crond.service",
	"f/lib/systemd/system/rescue.service",
	"f/lib/systemd/system/network.target",
	"f/lib/systemd/system/getty-pre.target",
	"f/lib/systemd/system/emac_dwc_eqos.service",
	"f/lib/systemd/system/dnsmasq.service",
	NULL
};

static char *systemd_files_7[] = {
	"f/lib/systemd/system-generators/systemd-fstab-generator",
	"d/lib/systemd/system/runlevel3.target.wants",
	"d/lib/systemd/system/runlevel4.target.wants",
	"f/lib/systemd/system/leprop.service",
	"f/lib/systemd/system/emergency.target",
	"f/lib/systemd/system/sshd.socket",
	"f/lib/systemd/system/qseecomd.service",
	"f/etc/initscripts/qseecomd",
	"f/lib/systemd/system/connman.service",
	"f/etc/systemd/system/chgrp-diag.service",
	"f/sbin/leprop-service",
	NULL
};

static char *systemd_files_8[] = {
	"f/lib/ld-2.31.so",
	"f/lib/ld-linux-aarch64.so.1",
	"f/etc/systemd/system.conf",
	"f/lib/systemd/system-generators/systemd-gpt-auto-generator",
	"f/usr/lib/liblzma.so.5.2.4",
	"f/usr/lib/liblzma.so.5",
	"f/lib/systemd/system/resize-userdata.service",
	"f/lib/systemd/system/systemd-fsck@.service",
	NULL
};

static char *systemd_files_9[] = {
	"f/lib/libz.so.1.2.11",
	"f/lib/libz.so.1",
	"f/lib/systemd/system/systemd-quotacheck.service",
	"f/lib/systemd/system/initrd-cleanup.service",
	"f/lib/systemd/system/initrd.target",
	"f/lib/systemd/system/local-fs.target",
	"f/lib/systemd/system/var-bluetooth.service",
	"f/lib/systemd/system/lxc-start.service",
	"f/lib/systemd/system/iptables.service",
	"f/lib/systemd/system/network-pre.target",
	"f/lib/systemd/system/slices.target",
	"d/lib/systemd/system/sysinit.target.wants",
	"f/lib/libcap.so.2.32",
	"f/lib/libcap.so.2",
	NULL
};

static char *systemd_files_10[] = {
	"f/lib/systemd/system/systemd-fsck-root.service",
	"f/lib/systemd/system/systemd-remount-fs.service",
	"f/lib/systemd/system/initrd-parse-etc.service",
	"f/lib/systemd/system/initrd-root-device.target",
	"f/lib/systemd/system/usb.service",
	"f/etc/systemd/system/thermal-engine.service",
	"f/lib/systemd/system/getty.target",
	"f/lib/systemd/system/serial-getty@.service",
	"f/lib/systemd/system/initrd-switch-root.service",
	"f/lib/systemd/system/paths.target",
	"d/lib/systemd/system/sockets.target.wants",
	"d/etc/systemd/system/sockets.target.wants",
	NULL
};

static char *systemd_files_11[] = {
	"f/lib/modprobe.d/systemd.conf",
	"f/lib/modules/4.14.180-perf/modules.dep.bin",
	"f/lib/modules/4.14.180-perf/modules.builtin.bin",
	"f/lib/systemd/system/systemd-tmpfiles-setup.service",
	NULL
};

static char *systemd_files_12[] = {
	"f/lib/systemd/libsystemd-shared-244.so",
	"f/etc/modprobe.d/blacklist.conf",
	"f/lib/modules/4.14.180-perf/modules.softdep",
	"f/lib/modules/4.14.180-perf/modules.alias.bin",
	"f/lib/modules/4.14.180-perf/modules.symbols.bin",
	"f/lib/systemd/system/systemd-journald-audit.socket",
	"f/lib/systemd/system/systemd-initctl.socket",
	NULL
};

static char *systemd_files_13[] = {
	"f/lib/libmount.so.1.1.0",
	"f/lib/libmount.so.1",
	"f/lib/systemd/system/sockets.target",
	"f/lib/systemd/system/systemd-networkd.socket",
	"f/lib/systemd/system/systemd-journald-dev-log.socket",
	"f/lib/systemd/system/systemd-journald.service",
	"f/lib/systemd/system/syslog.socket",
	"f/lib/systemd/system/systemd-journal-flush.service",
	"f/lib/systemd/system/var-data.service",
	"f/lib/systemd/system/var-build.prop.service",
	"f/lib/systemd/system/systemd-networkd.service",
	"f/lib/systemd/system/systemd-logind.service",
	"f/lib/systemd/system/systemd-ask-password-wall.service",
	"f/lib/systemd/system/systemd-networkd-wait-online.service",
	"f/lib/systemd/systemd-journald",
	"f/etc/init.d/emac_dwc_eqos_start_stop_le",
	"d/etc/systemd/system/local-fs.target.wants",
	"d/lib/systemd/system/runlevel2.target.wants",
	"d/lib/systemd/system/graphical.target.wants",
	"f/build.prop",
	NULL
};

static char *systemd_files_14[] = {
	"f/lib/libc-2.31.so",
	"f/lib/libc.so.6",
	"f/lib/systemd/system/systemd-update-done.service",
	"f/lib/systemd/system/systemd-udevd.service",
	"f/lib/systemd/system/systemd-udevd-kernel.socket",
	"f/lib/systemd/system/systemd-random-seed.service",
	"f/lib/systemd/system/systemd-modules-load.service",
	"f/lib/systemd/system/systemd-machine-id-commit.service",
	"f/lib/systemd/system/remote-fs-pre.target",
	"f/lib/systemd/system/var-usb.service",
	"f/lib/systemd/system/systemd-resolved.service",
	"f/lib/systemd/system/nss-lookup.target",
	"f/lib/systemd/system/strongswan.service",
	"f/lib/systemd/system/network-online.target",
	"f/lib/systemd/system/rngd.service",
	"f/lib/systemd/system/msm-bus.service",
	"d/etc/systemd/system/basic.target.wants",
	"d/lib/systemd/system/multi-user.target.wants",
	"d/lib/systemd/system/runlevel1.target.wants",
	"d/etc/systemd/system/network-online.target.wants",
	NULL
};

static char *systemd_files_15[] = {
	"f/lib/systemd/system/dbus.socket",
	"f/lib/systemd/system/systemd-udevd-control.socket",
	"f/lib/systemd/system/systemd-udev-trigger.service",
	"f/lib/systemd/system/time-set.target",
	"f/lib/systemd/system/systemd-sysusers.service",
	"f/lib/systemd/system/systemd-sysctl.service",
	"f/lib/systemd/system/var-smack-accesses.d.service",
	"f/lib/systemd/system/var-misc-wifi.service",
	"f/lib/systemd/system/systemd-user-sessions.service",
	"f/lib/systemd/system/nss-user-lookup.target",
	"f/lib/systemd/system/synergy.service",
	"f/lib/systemd/system/subsystem-ramdump.service",
	"f/lib/systemd/system/servicemanager.service",
	"f/usr/bin/servicemanager",
	"f/lib/systemd/system/lxc-init.service",
	NULL
};

static char *systemd_files_16[] = {
	"f/lib/systemd/system/systemd-initctl.service",
	"f/lib/systemd/system/tmp.mount",
	"f/lib/systemd/system/lxc.service",
	"f/lib/systemd/system/lxc-net.service",
	"d/lib/systemd/system/local-fs.target.wants",
	"d/lib/systemd/system/runlevel5.target.wants",
	"d/lib/systemd/system/rescue.target.wants",
	"f/lib/systemd/systemd-sysctl",
	NULL
};

static char **systemd_files[] = {
	systemd_files_1,
	systemd_files_2,
	systemd_files_3,
	systemd_files_4,
	systemd_files_5,
	systemd_files_6,
	systemd_files_7,
	systemd_files_8,
	systemd_files_9,
	systemd_files_10,
	systemd_files_11,
	systemd_files_12,
	systemd_files_13,
	systemd_files_14,
	systemd_files_15,
	systemd_files_16,
};

static char *weston_files_1[] = {
	NULL
};

static char *weston_files_2[] = {
	NULL
};

static char *weston_files_3[] = {
	NULL
};

static char *weston_files_4[] = {
	NULL
};

static char *weston_files_5[] = {
	NULL
};

static char *weston_files_6[] = {
	NULL
};

static char *weston_files_7[] = {
	NULL
};

static char *weston_files_8[] = {
	NULL
};

static char *weston_files_9[] = {
	NULL
};

static char *weston_files_10[] = {
	NULL
};

static char *weston_files_11[] = {
	NULL
};

static char *weston_files_12[] = {
	NULL
};

static char *weston_files_13[] = {
	NULL
};

static char *weston_files_14[] = {
	NULL
};

static char *weston_files_15[] = {
	NULL
};

static char *weston_files_16[] = {
	NULL
};

static char **weston_files[] = {
	weston_files_1,
	weston_files_2,
	weston_files_3,
	weston_files_4,
	weston_files_5,
	weston_files_6,
	weston_files_7,
	weston_files_8,
	weston_files_9,
	weston_files_10,
	weston_files_11,
	weston_files_12,
	weston_files_13,
	weston_files_14,
	weston_files_15,
	weston_files_16,
};

static char *lxc_files_1[] = {
	NULL
};

static char *lxc_files_2[] = {
	NULL
};

static char *lxc_files_3[] = {
	NULL
};

static char *lxc_files_4[] = {
	NULL
};

static char *lxc_files_5[] = {
	NULL
};

static char *lxc_files_6[] = {
	NULL
};

static char *lxc_files_7[] = {
	NULL
};

static char *lxc_files_8[] = {
	NULL
};

static char *lxc_files_9[] = {
	NULL
};

static char *lxc_files_10[] = {
	NULL
};

static char *lxc_files_11[] = {
	NULL
};

static char *lxc_files_12[] = {
	NULL
};

static char *lxc_files_13[] = {
	NULL
};

static char *lxc_files_14[] = {
	NULL
};

static char *lxc_files_15[] = {
	NULL
};

static char *lxc_files_16[] = {
	NULL
};

static char **lxc_files[] = {
	lxc_files_1,
	lxc_files_2,
	lxc_files_3,
	lxc_files_4,
	lxc_files_5,
	lxc_files_6,
	lxc_files_7,
	lxc_files_8,
	lxc_files_9,
	lxc_files_10,
	lxc_files_11,
	lxc_files_12,
	lxc_files_13,
	lxc_files_14,
	lxc_files_15,
	lxc_files_16,
};

static inline void prefetch_read_file(void *name, char *buf, int len)
{
	int fd;
	int num;

	fd = sys_open(name, O_RDONLY, 0);
	if (fd >= 0) {
		do {
			num = sys_read(fd, buf, len);
		} while (num);
		sys_close(fd);
	}
}

static inline void prefetch_read_dir(void *name, char *buf, int len)
{
	int fd;

	fd = sys_open(name, O_RDONLY, 0);
	if (fd >= 0) {
		sys_getdents64(fd, (struct linux_dirent64 *)buf, len);
		sys_close(fd);
	}
}

static int prefetch_thread(void *unused)
{
	struct cpumask cpumask;
	int thread_num = (int)unused;
	char *buf;
	int index;
	char *name;
	char **p_name;

	cpumask_clear(&cpumask);
	cpumask_set_cpu((thread_num%4)+1, &cpumask);
	if (sched_setaffinity(0, &cpumask))
		pr_err("setaffinity failed\n");

	buf = kzalloc(PREFETCH_BUF_LEN, GFP_KERNEL);
	printk("prefetchs%d %d\n", thread_num, smp_processor_id());

	index = 0;
	p_name = systemd_files[thread_num];
	name = p_name[index++];
	while (name) {
		if (*name == 'f')
			prefetch_read_file((void *)(name+1),
					buf, PREFETCH_BUF_LEN);
		else if (*name == 'd')
			prefetch_read_dir((void *)(name+1),
					buf, PREFETCH_BUF_LEN);
		else
			pr_err("invalid name %s\n", name);

		name = p_name[index++];
	}

	printk("prefetchm%d %d\n", thread_num, smp_processor_id());
	index = 0;
	p_name = weston_files[thread_num];
	name = p_name[index++];
	while (name) {
		if (*name == 'f')
			prefetch_read_file((void *)(name+1),
					buf, PREFETCH_BUF_LEN);
		else if (*name == 'd')
			prefetch_read_dir((void *)(name+1),
					buf, PREFETCH_BUF_LEN);
		else
			pr_err("invalid name %s\n", name);

		name = p_name[index++];
	}
	printk("prefetche%d %d\n", thread_num, smp_processor_id());
	index = 0;
	p_name = lxc_files[thread_num];
	name = p_name[index++];
	while (name) {
		if (*name == 'f')
			prefetch_read_file((void *)(name+1),
					buf, PREFETCH_BUF_LEN);
		else if (*name == 'd')
			prefetch_read_dir((void *)(name+1),
					buf, PREFETCH_BUF_LEN);
		else
			pr_err("invalid name %s\n", name);

		name = p_name[index++];
	}
	printk("prefetchl%d %d\n", thread_num, smp_processor_id());

	kfree(buf);
	return 0;
}

/*
 * Prepare the namespace - decide what/where to mount, load ramdisks, etc.
 */
void __init prepare_namespace(void)
{
	int is_floppy;
	int index;
	char name[16];
	static int first_time = 1;

	if (!first_time) {
	if (root_delay) {
		printk(KERN_INFO "Waiting %d sec before mounting root device...\n",
		       root_delay);
		ssleep(root_delay);
	}

	/*
	 * wait for the known devices to complete their probing
	 *
	 * Note: this is a potential source of long boot delays.
	 * For example, it is not atypical to wait 5 seconds here
	 * for the touchpad of a laptop to initialize.
	 */
	wait_for_device_probe();
	}

	if (first_time) {
	md_run_setup();
	dm_run_setup();

	if (saved_root_name[0]) {
		root_device_name = saved_root_name;
		if (!strncmp(root_device_name, "mtd", 3) ||
		    !strncmp(root_device_name, "ubi", 3)) {
			mount_block_root(root_device_name, root_mountflags);
			goto out;
		}
		ROOT_DEV = name_to_dev_t(root_device_name);
		if (strncmp(root_device_name, "/dev/", 5) == 0)
			root_device_name += 5;
	}

	if (initrd_load())
		goto out;

	/* wait for any asynchronous scanning to complete */
	if ((ROOT_DEV == 0) && root_wait) {
		printk(KERN_INFO "Waiting for root device %s...\n",
			saved_root_name);
		while (driver_probe_done() != 0 ||
			(ROOT_DEV = name_to_dev_t(saved_root_name)) == 0)
			msleep(5);
		async_synchronize_full();
	}

	is_floppy = MAJOR(ROOT_DEV) == FLOPPY_MAJOR;

	if (is_floppy && rd_doload && rd_load_disk(0))
		ROOT_DEV = Root_RAM0;

	mount_root();
	}
out:
	if (first_time) {
	devtmpfs_mount("dev");
	sys_mount(".", "/", NULL, MS_MOVE, NULL);
	sys_chroot(".");

	for (index = 0; index < PREFETCH_NUM; index++) {
		snprintf(name, 16, "prefetch%d", index);
		kthread_run(prefetch_thread, (void *)index, name);
	}
	}
	first_time = 0;
}

void __init early_prepare_namespace(char *name)
{
	int err;

	if (strstr(saved_root_name, name))
		prepare_namespace();
	else if (strstr(name, "vdc")) {
		err = sys_mount((char __user *)"/dev/vdc",
			(char __user *)"/var", (char __user *)"ext4", 0,
			(void __user *)NULL);
		if (err)
			pr_err("Mount Partition [%s] failed[%d]\n", name, err);
	}
}

static bool is_tmpfs;
static struct dentry *rootfs_mount(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	static unsigned long once;
	void *fill = ramfs_fill_super;

	if (test_and_set_bit(0, &once))
		return ERR_PTR(-ENODEV);

	if (IS_ENABLED(CONFIG_TMPFS) && is_tmpfs)
		fill = shmem_fill_super;

	return mount_nodev(fs_type, flags, data, fill);
}

static struct file_system_type rootfs_fs_type = {
	.name		= "rootfs",
	.mount		= rootfs_mount,
	.kill_sb	= kill_litter_super,
};

int __init init_rootfs(void)
{
	int err = register_filesystem(&rootfs_fs_type);

	if (err)
		return err;

	if (IS_ENABLED(CONFIG_TMPFS) && !saved_root_name[0] &&
		(!root_fs_names || strstr(root_fs_names, "tmpfs"))) {
		err = shmem_init();
		is_tmpfs = true;
	} else {
		err = init_ramfs_fs();
	}

	if (err)
		unregister_filesystem(&rootfs_fs_type);

	return err;
}

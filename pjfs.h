// SPDX-License-Identifier: GPL-3.0-only

#ifndef PJFS_H
#define PJFS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* on-disk structures */

struct pjfs_range {
	uint32_t begin;
	uint32_t end;
};

bool pjfs_range_in(const struct pjfs_range *self, const uint32_t x);
bool pjfs_range_overlaps(const struct pjfs_range *self, const struct pjfs_range *other);

struct pjfs_item_key {
	uint32_t virt_block_id;
	uint32_t type;
};

// offset zero
struct pjfs_superblock {
	char magic[16];
	bool is_big_endian;
	bool size_of_disksize;
	bool size_of_filesize;
	bool size_of_time;
	bool size_of_dir_header;
	bool size_of_dirent;
	bool size_of_item_header;
	char _17;
	uint32_t superblock_version;
	uint32_t superblock_size;
	uint32_t fs_version;
	uint32_t fs_flags;
	uint32_t block_size;
	char _2c[4];
	char _30[4];
	char _34[4];
	char _38[4];
	char _3c[4];
	char _40[4];
	char _44[4];
	char _48[4];
	char _4c[4];
	char _50[4];
	char _54[4];
	char _58[4];
	char _5c[4];
	char _60[4];
	char _64[4];
	char _68[4];
	char _6c[4];
	char _70[4];
	char _74[4];
	char _78[4];
	char _7c[4];
};

// directly after the superblock
// not sure about size
struct pjfs_volume_table_header {
	uint32_t crc32; // checksum of all following fields and all volume infos, or zero
	uint32_t num_vols;
	uint32_t size_of_volume_info;
	char _0c[4];
	char _10[4];
	char _14[4];
	char _18[4];
	char _1c[4];
};

// directly after the volume table header
struct pjfs_volume_info {
	char name[32];
	struct pjfs_range range;
	char _28[4];
	char _2c[4]; // journals?
	char _30[4]; // journals?
	char _34[4];
	struct pjfs_item_key slash_dir_key; // root directory
};

// first block of a volume contains these
struct pjfs_block_allocation_table_ptr {
	uint32_t phys_block_id; // 0xffffffff == end marker; points to a block ptr
	char _04[4]; // flags?
};

struct pjfs_block_ptr {
	uint32_t phys_block_id; // 0xffffffff == end marker
	uint32_t flags;
};

struct pjfs_item_header {
	char _00;
	char type; // 1 == file, 2 == directory
	char _02[2];
	struct pjfs_item_key key;
	struct pjfs_item_key parent_key; // == key for root directory
	char _14[4];
	uint64_t size;
	uint64_t min_allocation; // what's this?
	uint64_t max_allocation; // what's this?
	char _30[4];
	char _34[4];
	char _38[4];
	char _3c[4];
};

struct pjfs_dirent {
	struct pjfs_item_key key;
	char type; // 1 == file, 2 == directory
	char _09;
	char _0a[2];
	char _0c[4];
	char _10[4];
	char _14[4];
	char _18[4];
	char _1c[4];
	char name[32];
};

/* structures defined by this implementation */

struct pjfs_fs {
	char *buf;
	size_t size;
};

const struct pjfs_volume_table_header *pjfs_fs_volume_table_header(const struct pjfs_fs *self);
const struct pjfs_volume_info *pjfs_fs_volume_table(const struct pjfs_fs *self);

struct pjfs_volume {
	const struct pjfs_fs *fs;
	const struct pjfs_volume_info *info;
};

struct pjfs_volume pjfs_fs_volume(const struct pjfs_fs *self, const char *name);
const char *pjfs_volume_read_virt_block(const struct pjfs_volume *self, const uint32_t virt_block_id);

struct pjfs_slice {
	const char *ptr;
	size_t size;
};

struct pjfs_item {
	const struct pjfs_volume *vol;
	const struct pjfs_item_header *hdr;
};

struct pjfs_slice pjfs_item_read(const struct pjfs_item *self, const size_t offset);

int pjfs_directory_read(const struct pjfs_item *self, struct pjfs_dirent *dirent, const size_t offset);

#endif /* PJFS_H */

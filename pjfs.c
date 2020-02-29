// SPDX-License-Identifier: GPL-3.0-only

#include "pjfs.h"

#include <stdlib.h>
#include <string.h>

#define MIN(x, y) ((x) < (y) ? (x) : (y))

bool pjfs_range_in(const struct pjfs_range *self, const uint32_t x)
{
	return x >= self->begin && x < self->end;
}

bool pjfs_range_overlaps(const struct pjfs_range *self, const struct pjfs_range *other)
{
	return (self->begin >= other->begin && self->begin < other->end) ||
	       (other->begin >= self->begin && other->begin < self->end);
}

const struct pjfs_volume_table_header *pjfs_fs_volume_table_header(const struct pjfs_fs *self)
{
	return (const struct pjfs_volume_table_header *) (self->buf + sizeof (struct pjfs_superblock));
}

const struct pjfs_volume_info *pjfs_fs_volume_table(const struct pjfs_fs *self)
{
	return (const struct pjfs_volume_info *) (self->buf + sizeof (struct pjfs_superblock) + sizeof (struct pjfs_volume_table_header));
}
struct pjfs_volume pjfs_fs_volume(const struct pjfs_fs *self, const char *name)
{
	struct pjfs_volume result = {
		.fs = NULL,
		.info = NULL,
	};
	if (strlen(name) >= 32) {
		return result;
	}

	const uint32_t num_vols = pjfs_fs_volume_table_header(self)->num_vols;
	const struct pjfs_volume_info *info = pjfs_fs_volume_table(self);
	for (uint32_t i = 0; i < num_vols && i < 3; i++) {
		if (strcmp(info[i].name, name) == 0) {
			result.fs = self;
			result.info = info + i;
			return result;
		}
	}

	return result;
}

const char *pjfs_volume_read_virt_block(const struct pjfs_volume *self, const uint32_t virt_block_id)
{
	if ((virt_block_id / 64) >= 64) {
		// not implemented
		return NULL;
	}

	const char *start_of_volume = self->fs->buf + self->info->range.begin * 512;

	const struct pjfs_block_allocation_table_ptr batptr = ((struct pjfs_block_allocation_table_ptr *)start_of_volume)[virt_block_id / 64];

	if (batptr.phys_block_id == 0xffffffff) {
		return NULL;
	}

	if (!pjfs_range_in(&self->info->range, batptr.phys_block_id)) {
		return NULL;
	}

	const struct pjfs_block_ptr *bat = (const struct pjfs_block_ptr *)(self->fs->buf + batptr.phys_block_id * 512);
	const struct pjfs_block_ptr blockptr = bat[virt_block_id % 64];

	if (blockptr.phys_block_id == 0xffffffff) {
		return NULL;
	}

	if (!pjfs_range_in(&self->info->range, blockptr.phys_block_id)) {
		return NULL;
	}

	return self->fs->buf + blockptr.phys_block_id * 512;
}

struct pjfs_slice pjfs_item_read(const struct pjfs_item *self, const size_t offset) {
	struct pjfs_slice result = {
		.ptr = NULL,
		.size = 0,
	};

	if (offset >= self->hdr->size) {
		return result;
	}

	if (((uint32_t *)(self->hdr))[0x13] == 0xffffffff) {
		if (self->hdr->size > 0x1b0) {
			return result;
		}

		result.ptr = (char *)(self->hdr) + 0x50 + offset;
		result.size = self->hdr->size - offset;
		return result;
	} else {
		if (self->hdr->size > 0xd800) {
			// not implemented
			return result;
		}

		const uint32_t virt_block_id = ((uint32_t *)(self->hdr))[0x13 + offset / 0x200];

		if (virt_block_id == 0xffffffff) {
			return result;
		}
		const char *block = pjfs_volume_read_virt_block(self->vol, virt_block_id);

		result.ptr = block + offset % 0x200;
		result.size = MIN(self->hdr->size - offset, 0x200 - offset % 0x200);
		return result;
	}
}

int pjfs_directory_read(const struct pjfs_item *self, struct pjfs_dirent *dirent, const size_t offset)
{
	memset(dirent, 0, sizeof (struct pjfs_dirent));

	if (self->hdr->size % sizeof (struct pjfs_dirent) != 0) {
		return -1;
	}

	if (self->hdr->size == 0) {
		return -1;
	}

	if (offset > self->hdr->size / sizeof (struct pjfs_dirent)) {
		return 1;
	}

	const size_t start = (offset + 1) * sizeof (struct pjfs_dirent); // TODO this may overflow
	size_t i = 0;
	while (i < sizeof (struct pjfs_dirent)) {
		struct pjfs_slice slice = pjfs_item_read(self, i + start);
		if (slice.ptr == NULL) {
			return -1;
		}
		memcpy(((char *)dirent) + i, slice.ptr, MIN(sizeof (struct pjfs_dirent) - i, slice.size));
		i += slice.size;
	}
	return 0;
}

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define FUSE_USE_VERSION 31
#include <fuse3/fuse_lowlevel.h>

#include "pjfs.h"

#define UNUSED(x) (void)(x)
#define MIN(x, y) ((x) < (y) ? (x) : (y))

static uint32_t translate_block_id(const struct pjfs_volume *vol, const uint32_t id)
{
	if (id == vol->info->slash_dir_key.virt_block_id) {
		return 1;
	} else if (id == 1) {
		return vol->info->slash_dir_key.virt_block_id;
	} else {
		return id;
	}
}

static void pf_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	const struct pjfs_volume *vol = fuse_req_userdata(req);
	const char *block = pjfs_volume_read_virt_block(vol, translate_block_id(vol, parent));
	struct pjfs_item item = {
		.vol = vol,
		.hdr = (struct pjfs_item_header *)block,
	};

	if (block == NULL) {
		fuse_reply_err(req, ENOENT);
		return;
	}

	struct pjfs_dirent dirent;
	for (size_t i = 0; pjfs_directory_read(&item, &dirent, i) == 0; i++) {
		if (strcmp(dirent.name, name) == 0) {
			struct fuse_entry_param e;
			memset(&e, 0, sizeof(e));
			e.ino = translate_block_id(vol, dirent.key.virt_block_id);
			e.attr.st_ino = e.ino;
			e.attr.st_nlink = 1;
			e.attr.st_mode = 0400;
			if (dirent.type == 1) {
				e.attr.st_mode |= S_IFREG;
			} else if (dirent.type == 2) {
				e.attr.st_mode |= S_IFDIR;
			}
 
			fuse_reply_entry(req, &e);
			return;
		}
	}
	fuse_reply_err(req, ENOENT);
}

static void pf_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	UNUSED(fi);

	const struct pjfs_volume *vol = fuse_req_userdata(req);
	const struct pjfs_item_header *block = (struct pjfs_item_header *)pjfs_volume_read_virt_block(vol, translate_block_id(vol, ino));

	if (block == NULL) {
		fuse_reply_err(req, ENOENT);
		return;
	}

	struct stat st;
	memset(&st, 0, sizeof (struct stat));

	st.st_ino = ino;
	st.st_nlink = 1;
	st.st_size = block->size;
	st.st_mode = 0400;
	if (block->type == 1) {
		st.st_mode |= S_IFREG;
	} else if (block->type == 2) {
		st.st_mode |= S_IFDIR;
	}

	fuse_reply_attr(req, &st, 0.0);
}

static void pf_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi)
{
	UNUSED(fi);

	const struct pjfs_volume *vol = fuse_req_userdata(req);
	const char *block = pjfs_volume_read_virt_block(vol, translate_block_id(vol, ino));
	struct pjfs_item item = {
		.vol = vol,
		.hdr = (struct pjfs_item_header *)block,
	};

	if (block == NULL) {
		fuse_reply_err(req, ENOENT);
		return;
	}

	struct pjfs_dirent dirent;
	size_t oldsize = 0;
	struct dirbuf {
		char *p;
		size_t size;
	} b;

	memset(&b, 0, sizeof (struct dirbuf));
	for (size_t i = 0; pjfs_directory_read(&item, &dirent, i) == 0; i++) {
		b.size += fuse_add_direntry(req, NULL, 0, dirent.name, NULL, 0);
		b.p = (char *) realloc(b.p, b.size);
		struct stat st;
		memset(&st, 0, sizeof(struct stat));
		st.st_ino = translate_block_id(vol, dirent.key.virt_block_id);
		fuse_add_direntry(req, b.p + oldsize, b.size - oldsize, dirent.name, &st, b.size);
		oldsize = b.size;
	}

        if ((size_t)off < oldsize) {
                fuse_reply_buf(req, b.p + off, MIN(b.size - off, size));
	} else {
                fuse_reply_buf(req, NULL, 0);
	}
	free(b.p);
}

static void pf_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi)
{
	UNUSED(fi);

	const struct pjfs_volume *vol = fuse_req_userdata(req);
	const struct pjfs_item_header *block = (struct pjfs_item_header *)pjfs_volume_read_virt_block(vol, translate_block_id(vol, ino));

	if (block == NULL) {
		fuse_reply_err(req, ENOENT);
		return;
	}

	if (block->type == 2) {
		fuse_reply_err(req, EISDIR);
		return;
	}

	if (block->size <= (size_t)off) {
		fuse_reply_buf(req, NULL, 0);
		return;
	}

	const struct pjfs_item item = {
		.vol = vol,
		.hdr = block,
	};
	char *buf = malloc(MIN(block->size - off, size));
	if (buf == NULL) {
		fuse_reply_err(req, ENOMEM);
		return;
	}
	for (size_t i = off; i < block->size && i < size;) {
		const struct pjfs_slice slice = pjfs_item_read(&item, i);
		if (slice.ptr == NULL) {
			free(buf);
			fuse_reply_err(req, EAGAIN);
			return;
		}

		if (slice.size > block->size - i) {
			free(buf);
			fuse_reply_err(req, EIO);
			return;
		}

		memcpy(buf + i - off, slice.ptr, MIN(slice.size, MIN(block->size, size) - i));

		i += slice.size;
	}

	fuse_reply_buf(req, buf, MIN(block->size - off, size));
	free(buf);
}


static const struct fuse_lowlevel_ops ops = {
	.lookup = pf_lookup,
	.getattr = pf_getattr,
	.readdir = pf_readdir,
	.read = pf_read,
};

static void usage(const char *argv0)
{
	printf("usage: %s [options] <mountpoint>\n"
	       "    --path=<path>          path to the pjfs filesystem\n"
	       "    --volume=<volume>      which pjfs volume to mount\n",
	       argv0);
	fuse_cmdline_help();
	fuse_lowlevel_help();
}

int main(int argc, char **argv)
{
	int ret = 0;

	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse_cmdline_opts fuse_opts;

	if (fuse_parse_cmdline(&args, &fuse_opts) != 0) {
		fputs("invalid argument\n", stderr);
		ret = 1;
		goto end;
	}

	if (fuse_opts.show_help != 0) {
		usage(argv[0]);
		goto free_args;
	} else if (fuse_opts.show_version != 0) {
		fuse_lowlevel_version();
		goto free_args;
	}

	if (fuse_opts.mountpoint == NULL) {
		fputs("no mountpoint set\n", stderr);
		ret = 2;
		goto free_args;
	}

	struct options {
		const char *path;
		const char *volume;
	} pjfs_opts = {
		.path = NULL,
		.volume = NULL,
	};

	const struct fuse_opt pjfs_opts_spec[] = {
		{ "--path=%s", offsetof(struct options, path), 1 },
		{ "--volume=%s", offsetof(struct options, volume), 1 },
		FUSE_OPT_END,
	};

	if (fuse_opt_parse(&args, &pjfs_opts, (const struct fuse_opt *)&pjfs_opts_spec, NULL) != 0) {
		fputs("invalid argument\n", stderr);
		ret = 3;
		goto free_args;
	}

	if (pjfs_opts.path == NULL) {
		fputs("no path specified\n", stderr);
		ret = 4;
		goto free_args;
	}

	if (pjfs_opts.volume == NULL) {
		fputs("no volume specified\n", stderr);
		ret = 5;
		goto free_args;
	}

	struct stat st;
	if (stat(pjfs_opts.path, &st) != 0) {
		fputs("can't stat\n", stderr);
		ret = 6;
		goto free_args;
	}

	const int fd = open(pjfs_opts.path, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "can't open path: %s\n", strerror(errno));
		ret = 7;
		goto free_args;
	}

	char *buf = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	close(fd);
	if (buf == MAP_FAILED) {
		fputs("can't mmap\n", stderr);
		ret = 8;
		goto free_args;
	}

	const struct pjfs_fs fs = {
		.buf = buf,
		.size = st.st_size,
	};

	struct pjfs_volume vol = pjfs_fs_volume(&fs, pjfs_opts.volume);

	/*const char *block = pjfs_volume_read_virt_block(&vol, 169);
	struct pjfs_item item = {
		.vol = &vol,
		.hdr = block,
	};
	struct pjfs_slice sl = pjfs_item_read(&item, 512);
	printf("%d\n", sl.size);*/

	struct fuse_session *session = fuse_session_new(&args,
	                                                &ops,
	                                                sizeof (struct fuse_lowlevel_ops),
	                                                &vol);
	if (session == NULL) {
		fputs("can't create session\n", stderr);
		ret = 9;
		goto unmap;
	}

	if (fuse_set_signal_handlers(session) != 0) {
		fputs("can't set signal handlers\n", stderr);
		ret = 10;
		goto destroy_session;
	}

	if (fuse_session_mount(session, fuse_opts.mountpoint) != 0) {
		fputs("can't mount\n", stderr);
		ret = 11;
		goto remove_signal_handlers;
	}

	if (fuse_daemonize(fuse_opts.foreground) != 0) {
		fputs("can't daemonize\n", stderr);
		ret = 12;
		goto umount;
	}

	if (fuse_opts.singlethread != 0) {
		if (fuse_session_loop(session) != 0) {
			ret = 13;
		}
	} else {
		if (fuse_session_loop_mt(session, fuse_opts.clone_fd) != 0) {
			ret = 14;
		}
	}

umount:
	fuse_session_unmount(session);

remove_signal_handlers:
	fuse_remove_signal_handlers(session);

destroy_session:
	fuse_session_destroy(session);
	session = NULL;

unmap:
	munmap(buf, st.st_size);
	buf = NULL;

free_args:
	free(fuse_opts.mountpoint);
	fuse_opts.mountpoint = NULL;
	fuse_opt_free_args(&args);

end:
	return ret;
}

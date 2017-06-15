#include <sys/errno.h>
#include <unistd.h>
#include <fcntl.h>
#include "proto.h"
#include "mini-printf.h"
#include <minix/safecopies.h>
#include <minix/syslib.h>
#include <string.h>
#include "fat32.h"
#include <unistd.h>

#define FIND_HANDLE(type, h) \
	for (int i = 0; i < type##_handle_count; i++) { \
		if (type##_handles[i].nr == h) { \
			return &type##_handles[i]; \
		} \
	} \
return NULL

fat32_fs_t* find_fs_handle(int h) {
	FIND_HANDLE(fs, h);
}

fat32_dir_t* find_dir_handle(int h) {
	FIND_HANDLE(dir, h);
}

fat32_file_t* find_file_handle(int h) {
	FIND_HANDLE(file, h);
}

#define DESTROY_HANDLE(type, h) \
	fat32_##type##_t *handle = find_##type##_handle(h); \
if (handle != NULL) { \
	*handle = type##_handles[type##_handle_count]; \
	type##_handle_count--; \
}

void destroy_fs_handle(int h) {
	DESTROY_HANDLE(fs, h);
}

void destroy_dir_handle(int h) {
	DESTROY_HANDLE(dir, h);
}

void destroy_file_handle(int h) {
	DESTROY_HANDLE(file, h);
}

#define CREATE_HANDLE(type, handle) \
	do { \
		if (type##_handle_count >= FAT32_MAX_HANDLES) { \
			return -1; \
		} \
		int nr = type##_handle_next++; \
		handle = &type##_handles[type##_handle_count++]; \
		handle->nr = nr; \
	} while (0)

int do_open_fs(const char* device, endpoint_t who) {
	int ret = OK;
	fat32_fs_t *handle;
	CREATE_HANDLE(fs, handle);

	int fd = open(device, O_RDONLY);
	if (fd < 0) {
		ret = FAT32_ERR_IO;
		goto destroy_handle;
	}

	if ((ret = read_fat_header(fd, &handle->header)) != OK) {
		goto close_fd;
	}

	if ((ret = build_fat_info(&handle->header, &handle->info)) != OK)  {
		goto close_fd;
	}

	handle->is_open = TRUE;
	handle->fd = fd;
	handle->opened_by = who;

	return handle->nr;

close_fd:
	close(fd);

destroy_handle:
	fs_handle_count--;
	fs_handle_next--;

	return ret;
}

int do_open_root_directory(fat32_fs_t* fs, endpoint_t who) {
	int ret = OK;
	fat32_dir_t *handle;
	CREATE_HANDLE(dir, handle);

	char* buf = (char*) malloc(fs->info.bytes_per_cluster);
	if (!buf) {
		ret = ENOMEM;
		goto destroy_handle;
	}

	int cluster_nr = fs->header.ebr.root_cluster_nr;
	if ((ret = seek_read_cluster(&fs->header, &fs->info, fs->fd, cluster_nr, buf)) != OK) {
		goto dealloc_buffer;
	}

	handle->fs = fs;
	handle->active_cluster = cluster_nr;
	handle->cluster_buffer_offset = 0;
	handle->cluster_buffer = buf;

	return handle->nr;

dealloc_buffer:
	free(buf);

destroy_handle:
	dir_handle_count--;
	dir_handle_next--;

	return ret;
}

int advance_dir_cluster(fat32_dir_t* dir) {
	int ret, next_cluster_nr;
	if ((ret = get_next_cluster(&dir->fs->header, &dir->fs->info, dir->fs->fd,
					dir->active_cluster, &next_cluster_nr)) != OK) {
		return ret;
	}

	if (next_cluster_nr != -1) {
		dir->active_cluster = next_cluster_nr;
		dir->cluster_buffer_offset = 0;

		if ((ret = seek_read_cluster(&dir->fs->header, &dir->fs->info,
		                dir->fs->fd, next_cluster_nr, dir->cluster_buffer)) != OK) {
			return ret;
		}
	} else {
		dir->cluster_buffer_offset = -1;
	}

	return OK;
}

void filename_83_to_string(char* filename_83, char* dest) {
	// Copy the filename portion to the new string.
	strncpy(dest, filename_83, 8);

	// Find the first space in the filename and put a dot after it.
	int first_space;
	for (first_space = 0;
		 first_space < 8 && filename_83[first_space] != ' ';
		 first_space++)
		;

	dest[first_space] = '.';

	// Copy the extension right after the dot.
	strncpy(dest + first_space + 1, filename_83 + 8, 3);

	// Find the first space in te extension and put a null terminator after
	// it.
	for (first_space = 8;
		 first_space < 11 && filename_83[first_space] != ' ';
		 first_space++)
		;

	dest[first_space] = '\0';
}

int do_read_dir_entry(fat32_dir_t* dir, fat32_entry_t* dst, int* was_written,
		endpoint_t who)
{
	*was_written = FALSE;
	dir->last_entry_start_cluster = -1;
	dir->last_entry_was_dir = FALSE;

	if (dir->cluster_buffer_offset == -1) {
		// This whole dir was read previously.
		return OK;
	}

	int is_long_direntry = FALSE;
	char filename_buf[FAT32_MAX_NAME_LEN];
	memset(filename_buf, 0, FAT32_MAX_NAME_LEN);
	char *pfname = filename_buf + FAT32_MAX_NAME_LEN - 2; // Leave space for a single null terminator
	int seen_long_direntry = FALSE;

	fat32_direntry_t* short_entry = NULL;
	do {
		is_long_direntry = FALSE;

		if (dir->cluster_buffer_offset + 32 > dir->fs->info.bytes_per_cluster) {
			FAT_LOG_PRINTF(info, "Reached end of cluster %d", (int)dir->active_cluster);
			int ret;
			if ((ret = advance_dir_cluster(dir)) != OK) {
				return ret;
			}

			if (dir->cluster_buffer_offset == -1) {
				// No more clusters, we're done.
				return OK;
			}
		}

		// Choo choo! Violating strict aliasing rules. Compiler may decide to murder cat.
		fat32_any_direntry_t *direntry =
			(fat32_any_direntry_t*) &dir->cluster_buffer[dir->cluster_buffer_offset];
		dir->cluster_buffer_offset += 32;

		if (direntry->short_entry.filename_83[0] == '\0') {
			// Last directory entry in this cluster
			FAT_LOG_PRINTF(info, "Reached end of direntry in cluster %d", (int)dir->active_cluster);
			int ret;
			if ((ret = advance_dir_cluster(dir)) != OK) {
				return ret;
			}

			if (dir->cluster_buffer_offset == -1) {
				// No more clusters, we're done.
				return OK;
			}

			// If the previous calls were successful, we'll re-enter the loop
			// with a current_buffer_offset of 0 and a new cluster inside the
			// buffer.
		} else if (direntry->short_entry.attributes == 0x0f) {
			is_long_direntry = TRUE;
			seen_long_direntry = TRUE;

			// Copy the filename into a temporary buffer for easier handling
			// because its bytes are scattered around
			uint16_t temp_lfn_buf[16];
			uint16_t *pbuf = temp_lfn_buf;
			memset(temp_lfn_buf, 0, sizeof(temp_lfn_buf));

			for (int i = 0; i < 5; i++) {
				*pbuf++ = direntry->long_entry.chars_1[i];
			}
			for (int i = 0; i < 6; i++) {
				*pbuf++ = direntry->long_entry.chars_2[i];
			}
			for (int i = 0; i < 2; i++) {
				*pbuf++ = direntry->long_entry.chars_3[i];
			}

			// Find the length and copy everything into the filename
			// buffer
			int len = 0;
			for (uint16_t* len_pbuf = temp_lfn_buf; *len_pbuf; len_pbuf++) {
				len++;
			}

			for (uint16_t* pbuf_r = temp_lfn_buf + len - 1; pbuf_r >= temp_lfn_buf; pbuf_r--) {
				// Truncate 16 bits to only ASCII.
				*pfname-- = (char)(*pbuf_r & 0xff);
				if (pfname < filename_buf) {
					// The filename is too big for our buffer.
					FAT_LOG_PUTS(warn, "Long filename is too big for the buffer. Truncating.");
					continue;
				}
			}
		} else {
			short_entry = (fat32_direntry_t*) direntry;
		}
	} while (is_long_direntry);

	if (short_entry->attributes & FAT32_ATTR_DIR) {
		dir->last_entry_was_dir = TRUE;
	}

	int first_cluster_nr =
		(short_entry->first_cluster_nr_high << 8) |
		short_entry->first_cluster_nr_low;

	dir->last_entry_start_cluster = first_cluster_nr;

	memset(dst, 0, sizeof(fat32_entry_t));
	if (seen_long_direntry) {
		strncpy(dst->filename, pfname + 1, FAT32_MAX_NAME_LEN);
		dst->filename[FAT32_MAX_NAME_LEN - 1] = '\0';
	} else {
		filename_83_to_string(short_entry->filename_83, dst->filename);
	}

	convert_entry(short_entry, dst);

	FAT_LOG_PRINTF(debug, "Read %s '%s'", dst->is_directory ? "dir" : "file", dst->filename);
	*was_written = TRUE;

	return OK;
}

int do_open_directory(fat32_dir_t* source, endpoint_t who) {
	if (!source->last_entry_was_dir || source->last_entry_start_cluster < 0) {
		return EINVAL;
	}

	int ret = OK;
	fat32_dir_t *handle;
	CREATE_HANDLE(dir, handle);

	char* buf = (char*) malloc(source->fs->info.bytes_per_cluster);
	if (!buf) {
		ret = ENOMEM;
		goto destroy_handle;
	}

	int cluster_nr = source->last_entry_start_cluster;
	if ((ret = seek_read_cluster(&source->fs->header, &source->fs->info, source->fs->fd, cluster_nr, buf)) != OK) {
		goto dealloc_buffer;
	}

	handle->fs = source->fs;
	handle->active_cluster = cluster_nr;
	handle->cluster_buffer_offset = 0;
	handle->cluster_buffer = buf;

	return handle->nr;

dealloc_buffer:
	free(buf);

destroy_handle:
	dir_handle_count--;
	dir_handle_next--;

	return ret;
}

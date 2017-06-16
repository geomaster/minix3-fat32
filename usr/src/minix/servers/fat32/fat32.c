#include "proto.h"
#include "fat32.h"
#include "mini-printf.h"
#include <unistd.h>
#include <minix/com.h>
#include <string.h>

#define OK 0

/* Refer to the FAT32 documentation for details on the implementation of some of
 * these functions and some magic numbers used. */

int build_fat_info(fat32_header_t* header, fat32_info_t *dst_info) {
	if (header->bpb.header[0] != 0xEB || header->bpb.header[2] != 0x90) {
		return FAT32_ERR_NOT_FAT;
	}

	dst_info->bytes_per_cluster =
		header->bpb.bytes_per_sector * header->bpb.sectors_per_cluster;

	dst_info->fat_size_sectors =
		(header->bpb.sectors_per_table_16 == 0) ? header->ebr.sectors_per_table_32
		                                        : header->bpb.sectors_per_table_16;

	dst_info->root_dir_sectors =
		((header->bpb.root_direntries * 32) + (header->bpb.bytes_per_sector - 1)) / header->bpb.bytes_per_sector;

	dst_info->first_data_sector =
		header->bpb.reserved_sectors + (header->bpb.tables * dst_info->fat_size_sectors) +
		dst_info->root_dir_sectors;

	dst_info->first_fat_sector =
		header->bpb.reserved_sectors;

	int total_sectors =
		header->bpb.total_sectors_16 == 0 ? header->bpb.total_sectors_32
		                                  : header->bpb.total_sectors_16;

	int data_sectors =
		total_sectors - (header->bpb.reserved_sectors + (header->bpb.tables *
					                                     dst_info->fat_size_sectors) +
			             dst_info->root_dir_sectors);

	int total_clusters = data_sectors / header->bpb.sectors_per_cluster;
	if (total_clusters < 65525 || total_clusters >= 268435445) {
		// Is probably FAT, but not FAT32
		return FAT32_ERR_NOT_FAT;
	}

	return OK;
}

int read_fat_header(int fd, fat32_header_t* header) {
	size_t nread = read(fd, header, sizeof(fat32_header_t));
	if (nread < sizeof(fat32_header_t)) {
		return FAT32_ERR_IO;
	}

	return OK;
}

int seek_cluster(fat32_header_t* header, fat32_info_t* info, int fd, int
		cluster_nr)
{
	int first_sector_of_cluster =
		((cluster_nr - 2) * header->bpb.sectors_per_cluster) +
		info->first_data_sector;

	int first_byte_of_sector = first_sector_of_cluster * header->bpb.bytes_per_sector;

	off_t ret = lseek(fd, (off_t) first_byte_of_sector, SEEK_SET);
	if (ret != (off_t) first_byte_of_sector) {
		return FAT32_ERR_IO;
	}

	return OK;
}

int seek_read_cluster(fat32_header_t* header, fat32_info_t* info, int fd, int
		cluster_nr, char* buf)
{
	int ret;
	if ((ret = seek_cluster(header, info, fd, cluster_nr)) != OK) {
		return ret;
	}

	size_t nread = read(fd, buf, info->bytes_per_cluster);
	if (nread != info->bytes_per_cluster) {
		return FAT32_ERR_IO;
	}

	return OK;
}

int get_next_cluster(fat32_header_t* header, fat32_info_t* info, int fd, int
		cluster_nr, int* next_cluster_nr)
{
	// This can be cached for better speedz.
	uint32_t next_cluster;
	int fat_offset = cluster_nr * 4;
	int fat_entry_offset =
		(info->first_fat_sector * header->bpb.bytes_per_sector) +
		fat_offset;

	FAT_LOG_PRINTF(debug, "Seeking to offset %d", fat_entry_offset);
	off_t ret = lseek(fd, (off_t) fat_entry_offset, SEEK_SET);
	if (ret != (off_t) fat_entry_offset) {
		FAT_LOG_PRINTF(warn, "Seek to FAT at %d failed: %d.", fat_entry_offset, (int)ret);
		return FAT32_ERR_IO;
	}

	size_t nread = read(fd, &next_cluster, sizeof(uint32_t));
	if (nread != sizeof(uint32_t)) {
		FAT_LOG_PRINTF(warn, "Reading FAT entry at %d failed: %d.", fat_entry_offset, (int)nread);
		return FAT32_ERR_IO;
	}

	next_cluster &= 0x0fffffff;
	if (next_cluster >= 0x0ffffff8) {
		// Signal that this is the end of the cluster chain.
		FAT_LOG_PRINTF(debug, "No cluster follows cluster %d", cluster_nr);
		*next_cluster_nr = -1;
	} else {
		FAT_LOG_PRINTF(debug, "Cluster %d follows cluster %d", next_cluster, cluster_nr);
		*next_cluster_nr = next_cluster;
	}

	return OK;
}

void fat32_time_to_tm(fat32_time_t time, struct tm* t) {
	t->tm_hour = time.hours;
	t->tm_min = time.minutes;
	t->tm_sec = time.seconds * 2;
}

void fat32_date_to_tm(fat32_date_t date, struct tm* t) {
	t->tm_year = date.year + 80;
	t->tm_mon = date.month - 1;
	t->tm_mday = date.day;
}

void convert_entry(fat32_direntry_t* entry, fat32_entry_t* dest) {
	dest->is_directory = entry->attributes & FAT32_ATTR_DIR;
	dest->is_readonly = entry->attributes & FAT32_ATTR_READONLY;
	dest->is_hidden = entry->attributes & FAT32_ATTR_HIDDEN;
	dest->is_system = entry->attributes & FAT32_ATTR_SYSTEM;

	dest->size_bytes = entry->size_bytes;

	memset(&dest->modification, 0, sizeof(struct tm));
	fat32_date_to_tm(entry->last_modified_date, &dest->modification);
	fat32_time_to_tm(entry->last_modified_time, &dest->modification);

	memset(&dest->access, 0, sizeof(struct tm));
	fat32_date_to_tm(entry->last_access_date, &dest->access);

	memset(&dest->creation, 0, sizeof(struct tm));
	fat32_date_to_tm(entry->creation_date, &dest->creation);
	fat32_time_to_tm(entry->creation_time, &dest->creation);
}

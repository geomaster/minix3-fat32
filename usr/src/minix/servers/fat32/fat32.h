#pragma once
#include <stdint.h>

#ifndef _FAT32_H_FAT32_ENTRY_T_DEFINED
typedef struct fat32_entry_t fat32_entry_t;
#endif

typedef enum fat32_attrs_t {
	FAT32_ATTR_READONLY = 0x01,
	FAT32_ATTR_HIDDEN   = 0x02,
	FAT32_ATTR_SYSTEM   = 0x04,
	FAT32_ATTR_VOLUMEID = 0x08,
	FAT32_ATTR_DIR      = 0x10,
	FAT32_ATTR_ARCHIVE  = 0x20
} fat32_attrs_t;

typedef struct fat32_bpb_t {
	uint8_t   header[3];
	uint8_t   oem_id[8];
	uint16_t  bytes_per_sector;
	uint8_t   sectors_per_cluster;
	uint16_t  reserved_sectors;
	uint8_t   tables;
	uint16_t  root_direntries;
	uint16_t  total_sectors_16;
	uint8_t   media_descriptor_type;
	uint16_t  sectors_per_table_16;
	uint16_t  sectors_per_track;
	uint16_t  heads_or_sides;
	uint32_t  hidden_sectors;
	uint32_t  total_sectors_32;
} __attribute__((packed)) fat32_bpb_t;

typedef struct fat32_ebr_t {
	uint32_t  sectors_per_table_32;
	uint16_t  flags;
	uint16_t  version;
	uint32_t  root_cluster_nr;
	uint16_t  fsinfo_cluster_nr;
	uint16_t  backup_boot_cluster_nr;
	uint8_t   unused_1[12];
	uint8_t   drive_nr;
	uint8_t   unused_2;
	uint8_t   unused_3;
	uint32_t  volume_id;
	uint8_t   volume_label[11];
	uint8_t   fat_type_label[8];
} __attribute__((packed)) fat32_ebr_t;

typedef struct fat32_header_t {
	fat32_bpb_t bpb;
	fat32_ebr_t ebr;
} __attribute__((packed)) fat32_header_t;

typedef struct fat32_info_t {
	int fat_size_sectors;
	int root_dir_sectors;
	int first_data_sector;
	int first_fat_sector;
	int bytes_per_cluster;
} fat32_info_t;

typedef struct fat32_time_t {
	uint8_t hours   : 5;
	uint8_t minutes : 6;
	uint8_t seconds : 5;
} __attribute__((packed)) fat32_time_t;

typedef struct fat32_date_t {
	uint8_t year  : 7;
	uint8_t month : 4;
	uint8_t day   : 5;
} __attribute__((packed)) fat32_date_t;

typedef struct fat32_direntry_t {
	uint8_t       filename_83[11];
	uint8_t       attributes;
	uint8_t       reserved_1[2];
	fat32_time_t  creation_time;
	fat32_date_t  creation_date;
	fat32_date_t  last_access_date;
	uint16_t      first_cluster_nr_high;
	fat32_time_t  last_modified_time;
	fat32_date_t  last_modified_date;
	uint16_t      first_cluster_nr_low;
	uint32_t      size_bytes;
} __attribute__((packed)) fat32_direntry_t;

typedef struct fat32_lfn_direntry_t {
	uint8_t       ord;
	uint16_t      chars_1[5];
	uint8_t       attributes;
	uint8_t       lfn_type;
	uint8_t       checksum;
	uint16_t      chars_2[6];
	uint16_t      unused_1;
	uint16_t      chars_3[2];
} __attribute__((packed)) fat32_lfn_direntry_t;

typedef union fat32_any_direntry_t {
	fat32_direntry_t     short_entry;
	fat32_lfn_direntry_t long_entry;
} fat32_any_direntry_t;

int build_fat_info(fat32_header_t* header, fat32_info_t* dst_info);
int read_fat_header(int fd, fat32_header_t* dst);
int seek_cluster(fat32_header_t* header, fat32_info_t* info, int fd, int cluster_nr);
int seek_read_cluster(fat32_header_t* header, fat32_info_t* info, int fd, int cluster_nr, char* buf);
int get_next_cluster(fat32_header_t* header, fat32_info_t* info, int fd, int cluster_nr, int* next_cluster_nr);

// Caller sets the filename.
void convert_entry(fat32_direntry_t* entry, fat32_entry_t* dest);

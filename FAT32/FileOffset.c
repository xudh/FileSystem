#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <errno.h>

#define BOOTCODE_SIZE		448
#define BOOTCODE_FAT32_SIZE	420

struct msdos_volume_info {
    uint8_t drive_number;      /* BIOS drive number */
    uint8_t RESERVED;      /* Unused */
    uint8_t ext_boot_sign;     /* 0x29 if fields below exist (DOS 3.3+) */
    uint8_t volume_id[4];      /* Volume ID number */
    uint8_t volume_label[11];  /* Volume label */
    uint8_t fs_type[8];        /* Typically FAT12 or FAT16 */
} __attribute__ ((packed));

struct msdos_boot_sector {
    uint8_t boot_jump[3];      /* Boot strap short or near jump */
    uint8_t system_id[8];      /* Name - can be used to special case
                   partition manager volumes */
    uint16_t sector_size;    /* bytes per logical sector */
    uint8_t cluster_size;      /* sectors/cluster */
    uint16_t reserved;     /* reserved sectors */
    uint8_t fats;          /* number of FATs */
    uint8_t dir_entries[2];    /* root directory entries */
    uint8_t sectors[2];        /* number of sectors */
    uint8_t media;         /* media code (unused) */
    uint16_t fat_length;       /* sectors/FAT */
    uint16_t secs_track;       /* sectors per track */
    uint16_t heads;        /* number of heads */
    uint32_t hidden;       /* hidden sectors (unused) */
    uint32_t total_sect;       /* number of sectors (if sectors == 0) */
    union {
    struct {
        struct msdos_volume_info vi;
        uint8_t boot_code[BOOTCODE_SIZE];
    } __attribute__ ((packed)) _oldfat;
    struct {
        uint32_t fat32_length; /* sectors/FAT */
        uint16_t flags;    /* bit 8: fat mirroring, low 4: active fat */
        uint8_t version[2];    /* major, minor filesystem version */
        uint32_t root_cluster; /* first cluster in root directory */
        uint16_t info_sector;  /* filesystem info sector */
        uint16_t backup_boot;  /* backup boot sector */
        uint16_t reserved2[6]; /* Unused */
        struct msdos_volume_info vi;
        uint8_t boot_code[BOOTCODE_FAT32_SIZE];
    } __attribute__ ((packed)) _fat32;
    } __attribute__ ((packed)) fstype;

    uint16_t boot_sign;
} __attribute__ ((packed));

struct fat32_fsinfo {
    uint32_t reserved1;        /* Nothing as far as I can tell */
    uint32_t signature;        /* 0x61417272L */
    uint32_t free_clusters;    /* Free cluster count.  -1 if unknown */
    uint32_t next_cluster;     /* Most recently allocated cluster.
                 * Unused under Linux. */
    uint32_t reserved2[4];
};

struct msdos_dir_entry {
    char name[8], ext[3];   /* name and extension */
    uint8_t attr;          /* attribute bits */
    uint8_t lcase;         /* Case for base and extension */
    uint8_t ctime_ms;      /* Creation time, milliseconds */
    uint16_t ctime;        /* Creation time */
    uint16_t cdate;        /* Creation date */
    uint16_t adate;        /* Last access date */
    uint16_t starthi;      /* high 16 bits of first cl. (FAT32) */
    uint16_t time, date, start;    /* time, date and first cluster */
    uint32_t size;         /* file size (in bytes) */
} __attribute__ ((packed));

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("%s /dev/sd?? fileno\n", argv[0]);
		return -1;
	}

	FILE *pf = fopen(argv[1], "rb");
	if (pf == NULL)
	{
		printf("%d, errno = %d\n", __LINE__, errno);
		return -1;
	}

	struct msdos_boot_sector head = {{0}};
	if (fread(&head, sizeof head, 1, pf) != 1)
	{
		printf("%d, errno = %d\n", __LINE__, errno);
		return -1;
	}

	uint32_t rootDataOff = head.sector_size * (head.reserved + head.fats * head.fstype._fat32.fat32_length);
	printf("rootDataOff = %"PRIu32"\n", rootDataOff);
	uint32_t EntOff = rootDataOff + 0xC0 + atoi(argv[2]) * 0x40;
	printf("EntOff = %"PRIu32"\n", EntOff);
	if (fseek(pf, EntOff, SEEK_SET) != 0)
	{
		printf("%d, errno = %d\n", __LINE__, errno);
		return -1;
	}

	struct msdos_dir_entry ent = {{0}};
	if (fread(&ent, sizeof ent, 1, pf) != 1)
	{
		printf("%d, errno = %d\n", __LINE__, errno);
		return -1;
	}

	uint32_t fileCluster = (((uint32_t)ent.starthi)<<16) + ent.start;
	printf("fileCluster = %"PRIu32"\n", fileCluster);
	printf("head.fstype._fat32.root_cluster = %"PRIu32"\n", head.fstype._fat32.root_cluster);
	printf("head.cluster_size = %"PRIu32"\n", head.cluster_size);
	printf("head.sector_size = %"PRIu32"\n", head.sector_size);
	uint32_t fileOff = rootDataOff + (fileCluster - head.fstype._fat32.root_cluster) * head.cluster_size * head.sector_size;
	fclose(pf);
	printf("fileOff = %"PRIu32"\n", fileOff);
	return 0;
}


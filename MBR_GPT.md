parted工具给硬盘设备文件，创建一个msdos标签。

[root@(none) /home]$ ./parted --version
parted (GNU parted) 3.1
Copyright (C) 2012 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.

Written by <http://git.debian.org/?p=parted/parted.git;a=blob_plain;f=AUTHORS>.
[root@(none) /home]$ ./parted /dev/sda
GNU Parted 3.1
Using /dev/sda
Welcome to GNU Parted! Type 'help' to view a list of commands.
(parted) mklabel msdos                                                    
mklabel msdos
(parted) quit                                                             
quit
Information: You may need to update /etc/fstab.

[root@(none) /home]$ hexdump -C -n 512 /dev/sda 
00000000  fa b8 00 10 8e d0 bc 00  b0 b8 00 00 8e d8 8e c0  |................|
00000010  fb be 00 7c bf 00 06 b9  00 02 f3 a4 ea 21 06 00  |...|.........!..|
00000020  00 be be 07 38 04 75 0b  83 c6 10 81 fe fe 07 75  |....8.u........u|
00000030  f3 eb 16 b4 02 b0 01 bb  00 7c b2 80 8a 74 01 8b  |.........|...t..|
00000040  4c 02 cd 13 ea 00 7c 00  00 eb fe 00 00 00 00 00  |L.....|.........|
00000050  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000001b0  00 00 00 00 00 00 00 00  5a 58 03 00 00 00 00 00  |........ZX......|
000001c0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000001f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 55 aa  |..............U.|

00000200

可见msdos标签，写在硬盘第一扇区，55 aa是其的标记，(55 aa == 01010101 10101010)使用比的是较有序的01串。

这个扇区当然不只是放个标签用的，还充当引导扇区的作用，就是所谓的MBR(Master Boot Record)。其数据结构如下：

在linux代码中的头文件有定义

./linux-2.6.18/fs/partitions/efi.h中有

typedef struct _legacy_mbr {
        u8 boot_code[440];
        __le32 unique_mbr_signature;
        __le16 unknown;
        struct partition partition_record[4]; // 分区表也叫DPT(Disk Partition Table)
        __le16 signature;
} __attribute__ ((packed)) legacy_mbr;

其中的struct partition在
./linux-2.6.18/include/linux/genhd.h

struct partition {
        unsigned char boot_ind;         /* 0x80 - active */
        unsigned char head;             /* starting head */
        unsigned char sector;           /* starting sector */
        unsigned char cyl;              /* starting cylinder */
        unsigned char sys_ind;          /* What partition type */
        unsigned char end_head;         /* end head */
        unsigned char end_sector;       /* end sector */
        unsigned char end_cyl;          /* end cylinder */
        __le32 start_sect;      /* starting sector counting from 0 */
        __le32 nr_sects;                /* nr of sectors in partition */
} __attribute__((packed));  

sizeof (legacy_mbr)=512刚好填满一扇区。

从分区表数据结构中可以看出，同时包含C/H/S和LBA的寻址模式，start_sect就是LBA的开始地址。
注意sys_ind这个值是分区类型，而不一定是分区上的文件系统类型，如果没有代码的话可以用fdisk列出各自代表什么意思。下面增加两个分区

[root@(none) /home]$ ./parted /dev/sda
GNU Parted 3.1
Using /dev/sda
Welcome to GNU Parted! Type 'help' to view a list of commands.

(parted) mkpart primary fat32 1MB 4GB
(parted) mkpart extended 6GB 10GB 

(parted) quit                                                             
quit
Information: You may need to update /etc/fstab.

然后再看看第一扇区的数据。
[root@(none) /home]$ hexdump -C -n 512 /dev/sda 
00000000  fa b8 00 10 8e d0 bc 00  b0 b8 00 00 8e d8 8e c0  |................|
00000010  fb be 00 7c bf 00 06 b9  00 02 f3 a4 ea 21 06 00  |...|.........!..|
00000020  00 be be 07 38 04 75 0b  83 c6 10 81 fe fe 07 75  |....8.u........u|
00000030  f3 eb 16 b4 02 b0 01 bb  00 7c b2 80 8a 74 01 8b  |.........|...t..|
00000040  4c 02 cd 13 ea 00 7c 00  00 eb fe 00 00 00 00 00  |L.....|.........|
00000050  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000001b0  00 00 00 00 00 00 00 00  b1 ed 06 00 00 00 00 20  |............... |
000001c0  21 00 0c 57 71 e6 00 08  00 00 00 30 77 00 00 73  |!..Wq......0w..s|
000001d0  9b d9 0f fe ff ff 00 d0  b2 00 00 38 77 00 00 00  |...........8w...|
000001e0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000001f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 55 aa  |..............U.|
00000200

多了个分区表，有两条分区记录。mbr.partition_record[0].sys_ind==0x0c表示W95 FAT32& nbsp;(LBA)，
所以将mbr.partition_record[0].CHS相关的那些位清零也能正常工作，将 mbr.partition_record[0].LBA相关的那些位清零则不能。
若mbr.partition_record[0].sys_ind==0x0b应该是CHS模式的，后来的ext2/ntfs等文件系统只有LBA模式的。
C/H/S的值并非上面的mbr.partition_record[0].head/sector/cyl，因为Cylinder的值借用了Sector值的高2bit作为它自己的高2bit，
详细的算法见parted-3.1/libparted/labels/dos.c里面实现的 chs_get_cylinder, chs_get_head, chs_get_sector, chs_to_sector, sector_to_chs 这几个函数。

mbr.partition_record[1].sys_ind==0x0f表示W95 Ext'd (LBA)是个扩展分区。

mbr分区表决定硬盘顶多能有四个主分区(也就只能装4个操作系统)，有了扩展分区后可以有3个主分区+1个扩展分区( 上面能做N个逻辑分区)，
突破了分区数量的限制。然而LBA的2T硬盘容量限制，还是需要GPT(GUID Partition Table)分区模式。如下图：LBA0-N对应的就是扇区单位。

其数据结构也是在./linux-2.6.18/fs/partitions/efi.h中定义

typedef struct _gpt_header {
    __le64 signature;
    __le32 revision;
    __le32 header_size;
    __le32 header_crc32;
    __le32 reserved1;
    __le64 my_lba;
    __le64 alternate_lba;
    __le64 first_usable_lba;
    __le64 last_usable_lba;
    efi_guid_t disk_guid;
    __le64 partition_entry_lba;
    __le32 num_partition_entries;
    __le32 sizeof_partition_entry;
    __le32 partition_entry_array_crc32;
    u8 reserved2[GPT_BLOCK_SIZE - 92];
} __attribute__ ((packed)) gpt_header;

typedef struct _gpt_entry_attributes {
    u64 required_to_function:1;
    u64 reserved:47;
        u64 type_guid_specific:16;
} __attribute__ ((packed)) gpt_entry_attributes;

typedef struct _gpt_entry {
    efi_guid_t partition_type_guid;
    efi_guid_t unique_partition_guid;
    __le64 starting_lba;
    __le64 ending_lba;
    gpt_entry_attributes attributes;
    efi_char16_t partition_name[72 / sizeof (efi_char16_t)];
} __attribute__ ((packed)) gpt_entry;

有些变量类型是在./linux-2.6.18/include/linux/efi.h中定义。

很明显，GPT模式的LBA采用的是64位寻址，所以理论上支持(2^64 * 512)的容量，克服了2TB显示。
最大支持128个分区，4条分区记录就占用了一个扇区确实开销很大。需要特别留意的是
1、gpt_header.disk_guid, gpt_entry.unique_partition_guid都是独一无二的，因为从GPT全称看就知道，是靠这个来识别的。
2、gpt_header.num_partition_entries这个值一般等于128，分区记录的总条数，并非硬盘上真正有效的分区个数。
3、gpt_header.header_crc32这个值在计算前需先将自己置零。
4、Secondary GPT Header并非Primary GPT Header的简单备份，它描述的自己的Entry在哪等信息，尽管Entry内容是相同的。
详细的实现代码参考parted-3.1/libparted/labels/gpt.c
经有限的测试，发现没有Secondary GPT Header也能正常使用的。

给硬盘增加个gpt标签
[root@(none) /home]$ ./parted /dev/sda
GNU Parted 3.1
Using /dev/sda
Welcome to GNU Parted! Type 'help' to view a list of commands.
(parted) mklabel gpt                                                      
mklabel gpt

[root@(none) /home]$ hexdump -C -n 1024 /dev/sda
00000000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000001c0  01 00 ee fe ff ff 01 00  00 00 2f 60 38 3a00 00  |........../`8:..|
000001d0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000001f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 55 aa |..............U.|
00000200  45 46 49 20 50 41 52 54  00 00 01 00 5c 00 00 00  |EFI PART....\...|
00000210  3c 2d ba 4000 00 00 00  01 00 00 00 00 00 00 00  |<-.@............|
00000220  2f 60 38 3a 00 00 00 00  22 00 00 00 00 00 00 00  |/`8:....".......|
00000230  0e 60 38 3a 00 00 00 00  5c 66 43 b6 17 c6 5f 41  |.`8:....\fC..._A|
00000240  84 1e d0 de 15 8e fd f2  02 00 00 00 00 00 00 00  |................|
00000250  80 00 00 0080 00 00 00 86 d2 54 ab00 00 00 00  |..........T.....|
00000260  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00000400

可以看出第一扇区(LBA0)依然是MBR，后面再分析。第二个扇区(LBA1)开始是45 46 49 20 50 41 52 54这串是GPT标记，
因为其最早是EFI(Extensible Firmware Interface)项目中使用，所以这么称呼，上 面的数据结构在efi.h中也是这个原因的。
虽然没有分区，但gpt_header.num_partition_entries =& nbsp;80 00 00 00，Secondary GPT Header先不管它。下面给它增加一个分区
(parted) mkpart primary ext2 10MB 20GB                                    
mkpart primary ext2 10MB 20GB
[root@(none) /home]$ hexdump -C -n 1536 /dev/sda
00000000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000001c0  01 00 ee fe ff ff 01 00  00 00 2f 60 38 3a 00 00  |........../`8:..|
000001d0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000001f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 55 aa  |..............U.|
00000200  45 46 49 20 50 41 52 54  00 00 01 00 5c 00 00 00  |EFI PART....\...|
00000210  96 80 72 9700 00 00 00  01 00 00 00 00 00 00 00  |..r.............|
00000220  2f 60 38 3a 00 00 00 00  22 00 00 00 00 00 00 00  |/`8:....".......|
00000230  0e 60 38 3a 00 00 00 00  5c 66 43 b6 17 c6 5f 41  |.`8:....\fC..._A|
00000240  84 1e d0 de 15 8e fd f2  02 00 00 00 00 00 00 00  |................|
00000250  80 00 00 00 80 00 00 00  3e d4 69 7500 00 00 00  |........>.iu....|
00000260  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00000400  a2 a0 d0 eb e5 b9 33 44  87 c0 68 b6 b7 26 99 c7  |......3D..h..&..|
00000410  92 e7 b6 5c 70 67 ad 4d  9f d2 da f9 64 7b 56 bd  |...\pg.M....d{V.|
00000420  00 50 00 00 00 00 00 00  ff 07 54 02 00 00 00 00  |.P........T.....|
00000430  00 00 00 00 00 00 00 00  70 00 72 00 69 00 6d 00  |........p.r.i.m.|
00000440  61 00 72 00 79 00 00 00  00 00 00 00 00 00 00 00  |a.r.y...........|
00000450  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00000600

分区的信息从第三扇区(LBA2，确切的讲是gpt_header.partition_entry_lba)开始，同时分区的修改带动 gpt_header.partition_entry_array_crc32的变化，
又引起了gpt_header.header_crc32的变化，真不简单啊！

回头分析第一扇区(LBA0)上的MBR，其中mbr.partition_record[0] == 00 00 01 00 ee fe ff ff 01 00  00 00 2f 60 38 3a，
mbr.partition_record[0].sys_ind == 0xee表示GPT分区。如果是用EFI引导的计算机系统，像英特尔安腾处理器(Itanium)架构下，
先读取LBA1上的GPT头，获取分 区并引导操作系统。顶多在没找着正常的GPT头时，可能兼容性的去读取LBA0找MBR。
如果是传统的Bios引导的计算机系统，则先读取MBR，再在其 分区表中找到GPT类型的分区记录，然后再去找GPT头。
这种情况下，前面那个mbr.partition_record[0]就是必须的，创建一个 gpt标签时，类似于扩展分区，先创建分区记录，然后增加个GPT头。
再在GPT分区上创建多个GPT分区记录和在扩展分区上创建逻辑分区类似。
通过 mbr.partition_record[0].start_sect找到Primary GPT Header，而 mbr.partition_record[0].nr_sects是为了保护GPT分区的范围的。
因为gpt和mbr分区同时存在也是可能的，mbr.partition_record[0].nr_sects保户GPT分区不被mbr..partition_record[其它的]重叠。
所以LBA0上的MBR全称是Protective MBR。


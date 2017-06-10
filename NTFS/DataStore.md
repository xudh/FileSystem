文件系统概念很多，元文件很多，功能很丰富，实现比较复杂。参考http://zh.wikipedia.org/zh-cn/NTFS

MFT记录项由一个记录头和若干个属性组成的，NTFS文件系统的属性是描述文件各类信息的集合，其中的数据属性是描述文件的数据信息，如果文件很小，
那么其数据属性里直接存放有文件的数据，否则存放的就是“映射对数组mapping pairs array”，也就是用来指向实际存放数据的簇。
因为比较复杂，所以从结果开始分析，参考ntfs-3g_ntfsprogs-2012.1.15/ntfsprogs/mkntfs.c的代码，先编译mkntfs。
用mkntfs -f /dev/sda1快速格式化后，可以看到元文件在磁盘分区上的布局如下图：

可以看出很多元文件小，没有占用而外的簇，是因为被直接放在MFT中。还有这些元文件的顺序也不是按照MFT表中的记录项顺序的。

卷引导扇区：和其它文件系统类似，从中可以算出簇大小，本测试环境是4KB。

主文件表位图：使用max(一簇，8KB)的空间。
[root@localhost ~]$ hexdump -C -n 8192 -s 8k /dev/sda1
00002000  ff ff 00 07 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00002010  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00004000
其描述的是主文件表的记录项使用情况，记录项的数量超过8K*8时怎么处理？

主文件表：存放MFT记录项的表，一条记录默认占用1K的空间，其初始化的排列顺序如图1，第一条记录是它本身的。
[root@localhost ~]$ hexdump -C -n 1024 -s 16k /dev/sda1   
00004000  46 49 4c 45 30 00 03 00  00 00 00 00 00 00 00 00  |FILE0...........|
00004010  01 00 01 00 38 00 01 00  98 01 00 00 00 04 00 00  |....8...........|
00004020  00 00 00 00 00 00 00 00  04 00 00 00 00 00 00 00  |................|
00004030  02 00 00 00 00 00 00 00  10 00 00 00 60 00 00 00  |............`...|
00004040  00 00 18 00 00 00 00 00  48 00 00 00 18 00 00 00  |........H.......|
00004050  80 26 b9 64 57 3c ce 01  80 26 b9 64 57 3c ce 01  |.&.dW<...&.dW<..|
*
00004070  06 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00004080  00 00 00 00 00 01 00 00  00 00 00 00 00 00 00 00  |................|
00004090  00 00 00 00 00 00 00 00 30 00 00 00 68 00 00 00  |........0...h...|
000040a0  00 00 18 00 00 00 02 00  4a 00 00 00 18 00 01 00  |........J.......|
000040b0  05 00 00 00 00 00 05 00  80 26 b9 64 57 3c ce 01  |.........&.dW<..|
000040c0  80 26 b9 64 57 3c ce 01  80 26 b9 64 57 3c ce 01  |.&.dW<...&.dW<..|
000040d0  80 26 b9 64 57 3c ce 01  00 70 00 00 00 00 00 00  |.&.dW<...p......|
000040e0  00 6c 00 00 00 00 00 00  06 00 00 00 00 00 00 00  |.l..............|
000040f0  04 03 24 00 4d 00 46 00  54 00 00 00 00 00 00 00  |..$.M.F.T.......|
00004100  80 00 00 00 48 00 00 00  01 00 40 00 00 00 01 00  |....H.....@.....|
00004110  00 00 00 00 00 00 00 00  06 00 00 00 00 00 00 00  |................|
00004120  40 00 00 00 00 00 00 00  00 70 00 00 00 00 00 00  |@........p......|
00004130  00 6c 00 00 00 00 00 00  00 6c 00 00 00 00 00 00  |.l.......l......|
00004140  11 07 04 00 00 00 00 00  b0 00 00 00 48 00 00 00  |............H...|
00004150  01 00 40 00 00 00 03 00  00 00 00 00 00 00 00 00  |..@.............|
00004160  00 00 00 00 00 00 00 00  40 00 00 00 00 00 00 00  |........@.......|
00004170  00 10 00 00 00 00 00 00  08 00 00 00 00 00 00 00  |................|
00004180  08 00 00 00 00 00 00 00  11 01 02 00 00 00 00 00  |................|
00004190  ff ff ff ff 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000041a0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000041f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 02 00  |................|
00004200  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000043f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 02 00  |................|
00004400

0x00004000 - 0x0000402f是记录头，对应的数据结构如下：
typedef struct {
/*Ofs*/
/*  0   NTFS_RECORD; -- Unfolded here as gcc doesn't like unnamed structs. */
    NTFS_RECORD_TYPES magic;/* Usually the magic is "FILE". */
    u16 usa_ofs;        /* See NTFS_RECORD definition above. */
    u16 usa_count;      /* See NTFS_RECORD definition above. */

/*  8*/ LSN lsn;        /* $LogFile sequence number for this record.
                   Changed every time the record is modified. */
/* 16*/ u16 sequence_number;    /* Number of times this mft record has been
                   reused. (See description for MFT_REF
                   above.) NOTE: The increment (skipping zero)
                   is done when the file is deleted. NOTE: If
                   this is zero it is left zero. */
/* 18*/ u16 link_count;     /* Number of hard links, i.e. the number of
                   directory entries referencing this record.
                   NOTE: Only used in mft base records.
                   NOTE: When deleting a directory entry we
                   check the link_count and if it is 1 we
                   delete the file. Otherwise we delete the
                   FILE_NAME_ATTR being referenced by the
                   directory entry from the mft record and
                   decrement the link_count.
                   FIXME: Careful with Win32 + DOS names! */
/* 20*/ u16 attrs_offset;   /* Byte offset to the first attribute in this
                   mft record from the start of the mft record.
                   NOTE: Must be aligned to 8-byte boundary. */
/* 22*/ MFT_RECORD_FLAGS flags; /* Bit array of MFT_RECORD_FLAGS. When a file
                   is deleted, the MFT_RECORD_IN_USE flag is
                   set to zero. */
/* 24*/ u32 bytes_in_use;   /* Number of bytes used in this mft record.
                   NOTE: Must be aligned to 8-byte boundary. */
/* 28*/ u32 bytes_allocated;    /* Number of bytes allocated for this mft
                   record. This should be equal to the mft
                   record size. */
/* 32*/ MFT_REF base_mft_record; /* This is zero for base mft records.
                   When it is not zero it is a mft reference
                   pointing to the base mft record to which
                   this record belongs (this is then used to
                   locate the attribute list attribute present
                   in the base record which describes this
                   extension record and hence might need
                   modification when the extension record
                   itself is modified, also locating the
                   attribute list also means finding the other
                   potential extents, belonging to the non-base
                   mft record). */
/* 40*/ u16 next_attr_instance; /* The instance number that will be
                   assigned to the next attribute added to this
                   mft record. NOTE: Incremented each time
                   after it is used. NOTE: Every time the mft
                   record is reused this number is set to zero.
                   NOTE: The first instance number is always 0.
                 */
/* The below fields are specific to NTFS 3.1+ (Windows XP and above): */
/* 42*/ u16 reserved;       /* Reserved/alignment. */
/* 44*/ u32 mft_record_number;  /* Number of this mft record. */
/* sizeof() = 48 bytes */
/*
 * When (re)using the mft record, we place the update sequence array at this
 * offset, i.e. before we start with the attributes. This also makes sense,
 * otherwise we could run into problems with the update sequence array
 * containing in itself the last two bytes of a sector which would mean that
 * multi sector transfer protection wouldn't work. As you can't protect data
 * by overwriting it since you then can't get it back...
 * When reading we obviously use the data from the ntfs record header.
 */
} __attribute__((__packed__)) MFT_RECORD;
可以看出MFT_RECORD.attrs_offset=0x0038，也就后面是一条条属性了，其对应的数据结构如下：
typedef struct {
/*Ofs*/
/*  0*/ ATTR_TYPES type;    /* The (32-bit) type of the attribute. */
/*  4*/ u32 length;     /* Byte size of the resident part of the
                   attribute (aligned to 8-byte boundary).
                   Used to get to the next attribute. */
/*  8*/ u8 non_resident;    /* If 0, attribute is resident.
                   If 1, attribute is non-resident. */
/*  9*/ u8 name_length;     /* Unicode character size of name of attribute.
                   0 if unnamed. */
/* 10*/ u16 name_offset;    /* If name_length != 0, the byte offset to the
                   beginning of the name from the attribute
                   record. Note that the name is stored as a
                   Unicode string. When creating, place offset
                   just at the end of the record header. Then,
                   follow with attribute value or mapping pairs
                   array, resident and non-resident attributes
                   respectively, aligning to an 8-byte
                   boundary. */
/* 12*/ ATTR_FLAGS flags;   /* Flags describing the attribute. */
/* 14*/ u16 instance;       /* The instance of this attribute record. This
                   number is unique within this mft record (see
                   MFT_RECORD/next_attribute_instance notes
                   above for more details). */
/* 16*/ union {
        /* Resident attributes. */
        struct {
/* 16 */        u32 value_length; /* Byte size of attribute value. */
/* 20 */        u16 value_offset; /* Byte offset of the attribute
                           value from the start of the
                           attribute record. When creating,
                           align to 8-byte boundary if we
                           have a name present as this might
                           not have a length of a multiple
                           of 8-bytes. */
/* 22 */        RESIDENT_ATTR_FLAGS resident_flags; /* See above. */
/* 23 */        s8 reservedR;       /* Reserved/alignment to 8-byte
                           boundary. */
/* 24 */        void *resident_end[0]; /* Use offsetof(ATTR_RECORD,
                          resident_end) to get size of
                          a resident attribute. */
        } __attribute__((__packed__));
        /* Non-resident attributes. */
        struct {
/* 16*/         VCN lowest_vcn; /* Lowest valid virtual cluster number
                for this portion of the attribute value or
                0 if this is the only extent (usually the
                case). - Only when an attribute list is used
                does lowest_vcn != 0 ever occur. */
/* 24*/         VCN highest_vcn; /* Highest valid vcn of this extent of
                the attribute value. - Usually there is only one
                portion, so this usually equals the attribute
                value size in clusters minus 1. Can be -1 for
                zero length files. Can be 0 for "single extent"
                attributes. */
/* 32*/         u16 mapping_pairs_offset; /* Byte offset from the
                beginning of the structure to the mapping pairs
                array which contains the mappings between the
                vcns and the logical cluster numbers (lcns).
                When creating, place this at the end of this
                record header aligned to 8-byte boundary. */
/* 34*/         u8 compression_unit; /* The compression unit expressed
                as the log to the base 2 of the number of
                clusters in a compression unit. 0 means not
                compressed. (This effectively limits the
                compression unit size to be a power of two
                clusters.) WinNT4 only uses a value of 4. */
/* 35*/         u8 reserved1[5];    /* Align to 8-byte boundary. */
/* The sizes below are only used when lowest_vcn is zero, as otherwise it would
   be difficult to keep them up-to-date.*/
/* 40*/         s64 allocated_size; /* Byte size of disk space
                allocated to hold the attribute value. Always
                is a multiple of the cluster size. When a file
                is compressed, this field is a multiple of the
                compression block size (2^compression_unit) and
                it represents the logically allocated space
                rather than the actual on disk usage. For this
                use the compressed_size (see below). */
/* 48*/         s64 data_size;  /* Byte size of the attribute
                value. Can be larger than allocated_size if
                attribute value is compressed or sparse. */
/* 56*/         s64 initialized_size;   /* Byte size of initialized
                portion of the attribute value. Usually equals
                data_size. */
/* 64 */        void *non_resident_end[0]; /* Use offsetof(ATTR_RECORD,
                              non_resident_end) to get
                              size of a non resident
                              attribute. */
/* sizeof(uncompressed attr) = 64*/
/* 64*/         s64 compressed_size;    /* Byte size of the attribute
                value after compression. Only present when
                compressed. Always is a multiple of the
                cluster size. Represents the actual amount of
                disk space being used on the disk. */
/* 72 */        void *compressed_end[0];
                /* Use offsetof(ATTR_RECORD, compressed_end) to
                   get size of a compressed attribute. */
/* sizeof(compressed attr) = 72*/
        } __attribute__((__packed__));
    } __attribute__((__packed__));
} __attribute__((__packed__)) ATTR_RECORD;
其中的
typedef enum {
    AT_UNUSED           = const_cpu_to_le32(         0),
    AT_STANDARD_INFORMATION     = const_cpu_to_le32(      0x10),
    AT_ATTRIBUTE_LIST       = const_cpu_to_le32(      0x20),
    AT_FILE_NAME            = const_cpu_to_le32(      0x30),
    AT_OBJECT_ID            = const_cpu_to_le32(      0x40),
    AT_SECURITY_DESCRIPTOR      = const_cpu_to_le32(      0x50),
    AT_VOLUME_NAME          = const_cpu_to_le32(      0x60),
    AT_VOLUME_INFORMATION       = const_cpu_to_le32(      0x70),
    AT_DATA             = const_cpu_to_le32(      0x80),
    AT_INDEX_ROOT           = const_cpu_to_le32(      0x90),
    AT_INDEX_ALLOCATION     = const_cpu_to_le32(      0xa0),
    AT_BITMAP           = const_cpu_to_le32(      0xb0),
    AT_REPARSE_POINT        = const_cpu_to_le32(      0xc0),
    AT_EA_INFORMATION       = const_cpu_to_le32(      0xd0),
    AT_EA               = const_cpu_to_le32(      0xe0),
    AT_PROPERTY_SET         = const_cpu_to_le32(      0xf0),
    AT_LOGGED_UTILITY_STREAM    = const_cpu_to_le32(     0x100),
    AT_FIRST_USER_DEFINED_ATTRIBUTE = const_cpu_to_le32(    0x1000),
    AT_END              = const_cpu_to_le32(0xffffffff),
} ATTR_TYPES;
可以看出属性是按照属性类型号排序的，都有一个AT_END类型的属性结尾。从0x00004100 - 0x00004147是数据属性，而且是非常驻属性，
格式化选项没有开压缩功能，所以sizeof (ATTR_RECORD)==64，也就是后面的 0x00004140 - 0x00004147是什么呢？
ATTR_RECORD.name_offset==ATTR_RECORD.mapping_pairs_offset都指向它，但 ATTR_RECORD.name_length==0说明它不是名字，
而是"mapping pairs array"。其值 11 07 04，对应的数据结构是
struct map_pair
{
    u8 length_length:4;              //  后面length占用的字节数
    u8 start_length:4;               //  后面start占用的字节数
    u8/u16/u24/u32/.../u64 length;   // 长度，也就是占有多少个簇
    u8/u16/u24/u32/.../u64 start;    // 开始的逻辑簇号
}
因为后面两个成员的长度，由前面的值决定，所以无法定义实际的数据结构。前面的11 07 04说明length_length==1 得出length==07；
start_length==1得出start==04。也就是MFT的数据占用从4号簇开始的7簇，正好确认了簇是从分区头 开始编号的，而且27个元文件的MTF记录项各自占用1K，需要消耗7簇。

拿$MTFMirr的文件记录项再确认下
[root@localhost ~]$ hexdump -C -n 1024 -s 17k /dev/sda1
00004400  - 000044ff略
00004500  72 00 00 00 00 00 00 00  80 00 00 00 48 00 00 00  |r...........H...|
00004510  01 00 40 00 00 00 01 00  00 00 00 00 00 00 00 00  |..@.............|
00004520  00 00 00 00 00 00 00 00  40 00 00 00 00 00 00 00  |........@.......|
00004530  00 10 00 00 00 00 00 00  00 10 00 00 00 00 00 00  |................|
00004540  00 10 00 00 00 00 00 00  41 01 82 85 99 03 00 00  |........A.......|
*
后面略
*
可以看出0x00004508 - 0x0000454f是数据属性，其"mapping pairs array"是 41 01 82 85 99 03说明length_length==1得出 length==0x01；
start_length==4得出start==0x03998582，也就是MFTMirr的数据占用从 0x03998582号簇开始的1簇，大概卷的中间位置。
[root@localhost ~]$ hexdump -C -n 4096 -s 241571336k /dev/sda1出来看，就是MFT的前四条记录的备份。

当文件的数据所占用的簇不连续时，"映射对数组mapping pairs array"的成员就不只一个了，将顺序紧密排放在后面，到这里，可以看出NTFS文件找数据是B+树算法。

struct map_pair的
{
    u8/u16/u24/u32/.../u64 length;
    u8/u16/u24/u32/.../u64 start;
}部分也被称为运行时，其数组就是运行时列表了。当然ntfs-3g/runlist.h定义时扩充了下
struct _runlist_element {/* In memory vcn to lcn mapping structure element. */
    VCN vcn;    /* vcn = Starting virtual cluster number. */
    LCN lcn;    /* lcn = Starting logical cluster number. */
    s64 length; /* Run length in clusters. */
};
map_pair和runlist间的转换算法参考libntfs-3g/runlist.c:ntfs_mapping_pairs_build和ntfs_mapping_pairs_decompres


NTFS和大部分文件系统一样，也是将目录当做某类文件来出来，也是通过根目录层层查找到文件。接下来先找到MFT在根目录的记录：
[root@localhost ~]$ hexdump -C -n 1024 -s 21k /dev/sda1
00005400  46 49 4c 45 30 00 03 00  00 00 00 00 00 00 00 00  |FILE0...........|
00005410  05 00 01 00 38 00 03 00  00 02 00 00 00 04 00 00  |....8...........|
00005420  00 00 00 00 00 00 00 00  06 00 00 00 05 00 00 00  |................|
00005430  02 00 00 00 00 00 00 00  10 00 00 00 48 00 00 00  |............H...|
00005440  00 00 18 00 00 00 00 00  30 00 00 00 18 00 00 00  |........0.......|
00005450  80 26 b9 64 57 3c ce 01  80 26 b9 64 57 3c ce 01  |.&.dW<...&.dW<..|
*
00005470  26 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |&...............|
00005480  30 00 00 00 60 00 00 00  00 00 18 00 00 00 01 00  |0...`...........|
00005490  44 00 00 00 18 00 01 00  05 00 00 00 00 00 05 00  |D...............|
000054a0  80 26 b9 64 57 3c ce 01  80 26 b9 64 57 3c ce 01  |.&.dW<...&.dW<..|
*
000054c0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000054d0  06 00 00 10 00 00 00 00  01 03 2e 00 00 00 00 00  |................|
000054e0  50 00 00 00 48 00 00 00  01 00 40 00 00 00 02 00  |P...H.....@.....|
000054f0  00 00 00 00 00 00 00 00  01 00 00 00 00 00 00 00  |................|
00005500  40 00 00 00 00 00 00 00  00 20 00 00 00 00 00 00  |@........ ......|
00005510  2c 10 00 00 00 00 00 00  2c 10 00 00 00 00 00 00  |,.......,.......|
00005520  41 02 64 61 e6 00 00 00 90 00 00 00 58 00 00 00  |A.da........X...|
00005530  00 04 18 00 00 00 03 00  38 00 00 00 20 00 00 00  |........8... ...|
00005540  24 00 49 00 33 00 30 00  30 00 00 00 01 00 00 00  |$.I.3.0.0.......|
00005550  00 10 00 00 01 00 00 00  10 00 00 00 28 00 00 00  |............(...|
00005560  28 00 00 00 01 00 00 00  00 00 00 00 00 00 00 00  |(...............|
00005570  18 00 00 00 03 00 00 00  00 00 00 00 00 00 00 00  |................|
00005580  a0 00 00 00 50 00 00 00  01 04 40 00 00 00 05 00  |....P.....@.....|
00005590  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000055a0  48 00 00 00 00 00 00 00  00 10 00 00 00 00 00 00  |H...............|
000055b0  00 10 00 00 00 00 00 00  00 10 00 00 00 00 00 00  |................|
000055c0  24 00 49 00 33 00 30 00  41 01 66 61 e6 00 00 00  |$.I.3.0.A.fa....|
000055d0  b0 00 00 00 28 00 00 00  00 04 18 00 00 00 04 00  |....(...........|
000055e0  08 00 00 00 20 00 00 00  24 00 49 00 33 00 30 00  |.... ...$.I.3.0.|
000055f0  01 00 00 00 00 00 00 00  ff ff ff ff 00 00 02 00  |................|
00005600  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000057f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 02 00  |................|
00005800


目 录/文件夹的MFT_RECORD.flags会包含(或运 算)MFT_RECORD_IS_DIRECTORY     =  const_cpu_to_le16(0x0002),这个，所以上面的*0x00005416==0x0003。

留意下0x00005480 - 0x000054df是文件名属性，其值从0x00005490 - 0x000054df。
0x00005528 - 0x0x0000557f 是索引根AT_INDEX_ROOT属性，明显不够用，所以使用了0x00005580 - 0x000055cf是索引存储单元地 址AT_INDEX_ALLOCATION属性，
从映射对数组的成员 41 01 66 61 e6 00 ，可知其存在于0x00e66166号开始的1簇， 转储出来
[root@localhost ~]$ dd if=/dev/sda1 of=root_index bs=4K count=1 skip=15098214
[root@localhost ~]$ hexdump -C root_index 
00000000  49 4e 44 58 28 00 09 00  00 00 00 00 00 00 00 00  |INDX(...........|
00000010  00 00 00 00 00 00 00 00  28 00 00 00 d0 04 00 00  |........(.......|
00000020  e8 0f 00 00 00 00 00 00  03 00 ce 01 00 00 00 00  |................|
00000030  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000040  04 00 00 00 00 00 04 00  68 00 52 00 00 00 00 00  |........h.R.....|
00000050  05 00 00 00 00 00 05 00  80 26 b9 64 57 3c ce 01  |.........&.dW<..|
00000060  80 26 b9 64 57 3c ce 01  80 26 b9 64 57 3c ce 01  |.&.dW<...&.dW<..|
00000070  80 26 b9 64 57 3c ce 01  00 10 00 00 00 00 00 00  |.&.dW<..........|
00000080  00 0a 00 00 00 00 00 00  06 00 00 00 00 00 00 00  |................|
00000090  08 03 24 00 41 00 74 00  74 00 72 00 44 00 65 00  |..$.A.t.t.r.D.e.|
000000a0  66 00 00 00 00 00 01 00  08 00 00 00 00 00 08 00  |f...............|
000000b0  68 00 52 00 00 00 00 00  05 00 00 00 00 00 05 00  |h.R.............|
000000c0  80 26 b9 64 57 3c ce 01  80 26 b9 64 57 3c ce 01  |.&.dW<...&.dW<..|
*
000000e0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000000f0  06 00 00 00 00 00 00 00  08 03 24 00 42 00 61 00  |..........$.B.a.|
00000100  64 00 43 00 6c 00 75 00  73 00 00 00 00 00 07 00  |d.C.l.u.s.......|
00000110  06 00 00 00 00 00 06 00  60 00 50 00 00 00 00 00  |........`.P.....|
00000120  05 00 00 00 00 00 05 00  80 26 b9 64 57 3c ce 01  |.........&.dW<..|
00000130  80 26 b9 64 57 3c ce 01  80 26 b9 64 57 3c ce 01  |.&.dW<...&.dW<..|
00000140  80 26 b9 64 57 3c ce 01  00 70 e6 00 00 00 00 00  |.&.dW<...p......|
00000150  68 61 e6 00 00 00 00 00  06 00 00 00 00 00 00 00  |ha..............|
00000160  07 03 24 00 42 00 69 00  74 00 6d 00 61 00 70 00  |..$.B.i.t.m.a.p.|
00000170  - 000001cf略
000001d0  0b 00 00 00 00 00 0b 00  60 00 50 00 00 00 00 00  |........`.P.....|
000001e0  05 00 00 00 00 00 05 00  80 26 b9 64 57 3c ce 01  |.........&.dW<..|
000001f0  80 26 b9 64 57 3c ce 01  80 26 b9 64 57 3c 03 00  |.&.dW<...&.dW<..|
00000200  80 26 b9 64 57 3c ce 01  00 00 00 00 00 00 00 00  |.&.dW<..........|
00000210  00 00 00 00 00 00 00 00  06 00 00 10 00 00 00 00  |................|
00000220  07 03 24 00 45 00 78 00  74 00 65 00 6e 00 64 00  |..$.E.x.t.e.n.d.|
00000230  - 0000028f略
00000290  65 00 00 00 00 00 01 00  00 00 00 00 00 00 01 00  |e...............|
000002a0  60 00 4a 00 00 00 00 00  05 00 00 00 00 00 05 00  |`.J.............|
000002b0  80 26 b9 64 57 3c ce 01  80 26 b9 64 57 3c ce 01  |.&.dW<...&.dW<..|
*
000002d0  00 70 00 00 00 00 00 00  00 6c 00 00 00 00 00 00  |.p.......l......|
000002e0  06 00 00 00 00 00 00 00  04 03 24 00 4d 00 46 00  |..........$.M.F.|
000002f0  54 00 00 00 00 00 00 00  01 00 00 00 00 00 01 00  |T...............
00000300  - 0000047f略
00000480  05 00 00 00 00 00 05 00  58 00 44 00 00 00 00 00  |........X.D.....|
00000490  05 00 00 00 00 00 05 00  80 26 b9 64 57 3c ce 01  |.........&.dW<..|
000004a0  80 26 b9 64 57 3c ce 01  80 26 b9 64 57 3c ce 01  |.&.dW<...&.dW<..|
000004b0  80 26 b9 64 57 3c ce 01  00 00 00 00 00 00 00 00  |.&.dW<..........|
000004c0  00 00 00 00 00 00 00 00  06 00 00 10 00 00 00 00  |................|
000004d0  01 03 2e 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000004e0  10 00 00 00 02 00 00 00  00 00 00 00 00 00 00 00  |................|
000004f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*后面略

可以看出根目录/文件夹下的索引是按文件名排序的，根目录本身的索引放在最后。其中索引的数据结构如下：
typedef struct {
/*  0   INDEX_ENTRY_HEADER; -- Unfolded here as gcc dislikes unnamed structs. */
    union {     /* Only valid when INDEX_ENTRY_END is not set. */
        MFT_REF indexed_file;       /* The mft reference of the file
                           described by this index
                           entry. Used for directory
                           indexes. */
        struct { /* Used for views/indexes to find the entry's data. */
            u16 data_offset;    /* Data byte offset from this
                           INDEX_ENTRY. Follows the
                           index key. */
            u16 data_length;    /* Data length in bytes. */
            u32 reservedV;      /* Reserved (zero). */
        } __attribute__((__packed__));
    } __attribute__((__packed__));
/*  8*/ u16 length;      /* Byte size of this index entry, multiple of
                    8-bytes. Size includes INDEX_ENTRY_HEADER
                    and the optional subnode VCN. See below. */
/* 10*/ u16 key_length;      /* Byte size of the key value, which is in the
                    index entry. It follows field reserved. Not
                    multiple of 8-bytes. */
/* 12*/ INDEX_ENTRY_FLAGS ie_flags; /* Bit field of INDEX_ENTRY_* flags. */
/* 14*/ u16 reserved;        /* Reserved/align to 8-byte boundary. */
/*  End of INDEX_ENTRY_HEADER */
/* 16*/ union {     /* The key of the indexed attribute. NOTE: Only present
               if INDEX_ENTRY_END bit in flags is not set. NOTE: On
               NTFS versions before 3.0 the only valid key is the
               FILE_NAME_ATTR. On NTFS 3.0+ the following
               additional index keys are defined: */
        FILE_NAME_ATTR file_name;/* $I30 index in directories. */
        SII_INDEX_KEY sii;  /* $SII index in $Secure. */
        SDH_INDEX_KEY sdh;  /* $SDH index in $Secure. */
        GUID object_id;     /* $O index in FILE_Extend/$ObjId: The
                       object_id of the mft record found in
                       the data part of the index. */
        REPARSE_INDEX_KEY reparse;  /* $R index in
                           FILE_Extend/$Reparse. */
        SID sid;        /* $O index in FILE_Extend/$Quota:
                       SID of the owner of the user_id. */
        u32 owner_id;       /* $Q index in FILE_Extend/$Quota:
                       user_id of the owner of the quota
                       control entry in the data part of
                       the index. */
    } __attribute__((__packed__)) key;
    /* The (optional) index data is inserted here when creating.
    VCN vcn;       If INDEX_ENTRY_NODE bit in ie_flags is set, the last
               eight bytes of this index entry contain the virtual
               cluster number of the index block that holds the
               entries immediately preceding the current entry. 
    
               If the key_length is zero, then the vcn immediately
               follows the INDEX_ENTRY_HEADER. 
    
               The address of the vcn of "ie" INDEX_ENTRY is given by 
               (char*)ie + le16_to_cpu(ie->length) - sizeof(VCN)
    */
} __attribute__((__packed__)) INDEX_ENTRY;

通过INDEX_ENTRY.indexed_file可以知道本索引在MFT表中的记录号，然后找到它。NTFS文件系统查找文件是通过根目录的MFT记录项找到根目录下的索引项块，
匹配文件(夹)名找到对应的索引项，然后根据索引项找到该文件(夹)在MFT中的记录项，然后层层查找。
其中的
typedef struct {
/*hex ofs*/
/*  0*/ MFT_REF parent_directory;   /* Directory this filename is
                       referenced from. */
/*  8*/ s64 creation_time;      /* Time file was created. */
/* 10*/ s64 last_data_change_time;  /* Time the data attribute was last
                       modified. */
/* 18*/ s64 last_mft_change_time;   /* Time this mft record was last
                       modified. */
/* 20*/ s64 last_access_time;       /* Last time this mft record was
                       accessed. */
/* 28*/ s64 allocated_size;     /* Byte size of on-disk allocated space
                       for the data attribute.  So for
                       normal $DATA, this is the
                       allocated_size from the unnamed
                       $DATA attribute and for compressed
                       and/or sparse $DATA, this is the
                       compressed_size from the unnamed
                       $DATA attribute.  NOTE: This is a
                       multiple of the cluster size. */
/* 30*/ s64 data_size;          /* Byte size of actual data in data
                       attribute. */
/* 38*/ FILE_ATTR_FLAGS file_attributes;    /* Flags describing the file. */
/* 3c*/ union {
    /* 3c*/ struct {
        /* 3c*/ u16 packed_ea_size; /* Size of the buffer needed to
                           pack the extended attributes
                           (EAs), if such are present.*/
        /* 3e*/ u16 reserved;       /* Reserved for alignment. */
        } __attribute__((__packed__));
    /* 3c*/ u32 reparse_point_tag;      /* Type of reparse point,
                           present only in reparse
                           points and only if there are
                           no EAs. */
    } __attribute__((__packed__));
/* 40*/ u8 file_name_length;            /* Length of file name in
                           (Unicode) characters. */
/* 41*/ FILE_NAME_TYPE_FLAGS file_name_type;    /* Namespace of the file name.*/
/* 42*/ ntfschar file_name[0];          /* File name in Unicode. */
} __attribute__((__packed__)) FILE_NAME_ATTR;
和上面MFT中的文件名属性的值是一样的，便于快速列出目录/文件夹下的文件基本信息。

0x000004d8 - 0x000004e7也是个索引实体，其INDEX_ENTRY.ie_flags==0x0002，根据枚举定义
typedef enum {
    INDEX_ENTRY_NODE = const_cpu_to_le16(1), /* This entry contains a
                    sub-node, i.e. a reference to an index
                    block in form of a virtual cluster
                    number (see below). */
    INDEX_ENTRY_END  = const_cpu_to_le16(2), /* This signifies the last
                    entry in an index block. The index
                    entry does not represent a file but it
                    can point to a sub-node. */
    INDEX_ENTRY_SPACE_FILLER = 0xffff, /* Just to force 16-bit width. */
} __attribute__((__packed__)) INDEX_ENTRY_FLAGS;
可知它是个结束索引体。

创建普通的文件、文件夹做实验

[root@localhost ~]$ ntfs-3g /dev/sda1 /var/
[root@localhost ~]$ mkdir /var/testDir
[root@localhost ~]$ echo hello world! > /var/testDir/file1
[root@localhost ~]$ echo QWERT1234509876 > "/var/(file0.txt"
[root@localhost ~]$ umount /var/
[root@localhost ~]$ hexdump -C -n 8192 -s 8k /dev/sda1
00002000  ff ff 00 07 00 00 00 00  07 00 00 00 00 00 00 00  |................|
00002010  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00004000
新占用了0x40, 0x41, 0x42号MFT记录项。

[root@localhost ~]$ dd if=/dev/sda1 of=root_index bs=4K count=1 skip=15098214
[root@localhost ~]$ hexdump -C root_index 
00000000  - 0000047f略
00000480  42 00 00 00 00 00 01 00  68 00 56 00 00 00 00 00  |B.......h.V.....|
00000490  05 00 00 00 00 00 05 00  4a 90 e6 cc e8 3d ce 01  |........J....=..|
000004a0  f8 a5 e6 cc e8 3d ce 01  f8 a5 e6 cc e8 3d ce 01  |.....=.......=..|
000004b0  4a 90 e6 cc e8 3d ce 01  10 00 00 00 00 00 00 00  |J....=..........|
000004c0  10 00 00 00 00 00 00 00  20 00 00 00 00 00 00 00  |........ .......|
000004d0  0a 00 28 00 66 00 69 00  6c 00 65 00 30 00 2e 00  |..(.f.i.l.e.0...|
000004e0  74 00 78 00 74 00 00 00  05 00 00 00 00 00 05 00  |t.x.t...........|
000004f0  58 00 44 00 00 00 00 00  05 00 00 00 00 00 05 00  |X.D.............|
00000500  80 26 b9 64 57 3c ce 01  88 96 e6 cc e8 3d ce 01  |.&.dW<.......=..|
00000510  88 96 e6 cc e8 3d ce 01  95 46 55 cf e8 3d ce 01  |.....=...FU..=..|
00000520  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000530  26 00 00 10 00 00 00 00  01 03 2e 00 00 00 00 00  |&...............|
00000540  40 00 00 00 00 00 01 00  60 00 50 00 00 00 00 00  |@.......`.P.....|
00000550  05 00 00 00 00 00 05 00  01 e2 ab d0 e6 3d ce 01  |.............=..|
00000560  37 35 7b f1 e6 3d ce 01  37 35 7b f1 e6 3d ce 01  |75{..=..75{..=..|
00000570  01 e2 ab d0 e6 3d ce 01  00 00 00 00 00 00 00 00  |.....=..........|
00000580  00 00 00 00 00 00 00 00  20 00 00 10 00 00 00 00  |........ .......|
00000590  07 00 74 00 65 00 73 00  74 00 44 00 69 00 72 00  |..t.e.s.t.D.i.r.|
000005a0  00 00 00 00 00 00 00 00  10 00 00 00 02 00 00 00  |................|
*后面略

可以看出根据文件名排序，(file0.txt这个文件的索引被排到根目录前面去了。(file0.txt和testDir的 INDEX_ENTRY.indexed_file分别
为0x0001000000000042和0x0001000000000040，据此能找到其在 MFT中的记录项。
[root@localhost ~]$ hexdump -C -n 3072 -s 80k /dev/sda1  
00014000  46 49 4c 45 30 00 03 00  00 00 00 00 00 00 00 00  |FILE0...........|
00014010  01 00 01 00 38 00 03 00  08 02 00 00 00 04 00 00  |....8...........|
00014020  00 00 00 00 00 00 00 00  04 00 00 00 40 00 00 00  |............@...|
00014030  05 00 00 00 00 00 00 00  10 00 00 00 48 00 00 00  |............H...|
00014040  00 00 00 00 00 00 00 00  30 00 00 00 18 00 00 00  |........0.......|
00014050  01 e2 ab d0 e6 3d ce 01  37 35 7b f1 e6 3d ce 01  |.....=..75{..=..|
00014060  37 35 7b f1 e6 3d ce 01  01 e2 ab d0 e6 3d ce 01  |75{..=.......=..|
00014070  20 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  | ...............|
00014080  30 00 00 00 68 00 00 00  00 00 00 00 00 00 03 00  |0...h...........|
00014090  50 00 00 00 18 00 01 00  05 00 00 00 00 00 05 00  |P...............|
000140a0  01 e2 ab d0 e6 3d ce 01  01 e2 ab d0 e6 3d ce 01  |.....=.......=..|
*
000140c0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000140d0  20 00 00 10 00 00 00 00  07 00 74 00 65 00 73 00  | .........t.e.s.|
000140e0  74 00 44 00 69 00 72 00  50 00 00 00 68 00 00 00  |t.D.i.r.P...h...|
000140f0  00 00 00 00 00 00 01 00  50 00 00 00 18 00 00 00  |........P.......|
00014100  01 00 04 80 14 00 00 00  24 00 00 00 00 00 00 00  |........$.......|
00014110  34 00 00 00 01 02 00 00  00 00 00 05 20 00 00 00  |4........... ...|
00014120  20 02 00 00 01 02 00 00  00 00 00 05 20 00 00 00  | ........... ...|
00014130  20 02 00 00 02 00 1c 00  01 00 00 00 00 03 14 00  | ...............|
00014140  ff 01 1f 00 01 01 00 00  00 00 00 01 00 00 00 00  |................|
00014150  90 00 00 00 b0 00 00 00  00 04 18 00 00 00 02 00  |................|
00014160  90 00 00 00 20 00 00 00  24 00 49 00 33 00 30 00  |.... ...$.I.3.0.|
00014170  30 00 00 00 01 00 00 00  00 10 00 00 01 00 00 00  |0...............|
00014180  10 00 00 00 80 00 00 00  80 00 00 00 00 00 00 00  |................|
00014190  41 00 00 00 00 00 01 00  60 00 4c 00 00 00 00 00  |A.......`.L.....|
000141a0  40 00 00 00 00 00 01 00  f6 31 7b f1 e6 3d ce 01  |@........1{..=..|
000141b0  64 43 7b f1 e6 3d ce 01  64 43 7b f1 e6 3d ce 01  |dC{..=..dC{..=..|
000141c0  f6 31 7b f1 e6 3d ce 01  10 00 00 00 00 00 00 00  |.1{..=..........|
000141d0  0d 00 00 00 00 00 00 00  20 00 00 00 00 00 00 00  |........ .......|
000141e0  05 00 66 00 69 00 6c 00  65 00 31 00 00 00 00 00  |..f.i.l.e.1.....|
000141f0  00 00 00 00 00 00 00 00  10 00 00 00 02 00 05 00  |................|
00014200  ff ff ff ff 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00014210  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000143f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 05 00  |................|
00014400  46 49 4c 45 30 00 03 00  00 00 00 00 00 00 00 00  |FILE0...........|
00014410  01 00 01 00 38 00 01 00  80 01 00 00 00 04 00 00  |....8...........|
00014420  00 00 00 00 00 00 00 00  04 00 00 00 41 00 00 00  |............A...|
00014430  05 00 00 00 00 00 00 00  10 00 00 00 48 00 00 00  |............H...|
00014440  00 00 00 00 00 00 00 00  30 00 00 00 18 00 00 00  |........0.......|
00014450  f6 31 7b f1 e6 3d ce 01  64 43 7b f1 e6 3d ce 01  |.1{..=..dC{..=..|
00014460  64 43 7b f1 e6 3d ce 01  f6 31 7b f1 e6 3d ce 01  |dC{..=...1{..=..|
00014470  20 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  | ...............|
00014480  30 00 00 00 68 00 00 00  00 00 00 00 00 00 03 00  |0...h...........|
00014490  4c 00 00 00 18 00 01 00  40 00 00 00 00 00 01 00  |L.......@.......|
000144a0  f6 31 7b f1 e6 3d ce 01  f6 31 7b f1 e6 3d ce 01  |.1{..=...1{..=..|
*
000144c0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000144d0  20 00 00 00 00 00 00 00  05 00 66 00 69 00 6c 00  | .........f.i.l.|
000144e0  65 00 31 00 00 00 00 00  50 00 00 00 68 00 00 00  |e.1.....P...h...|
000144f0  00 00 00 00 00 00 01 00  50 00 00 00 18 00 00 00  |........P.......|
00014500  01 00 04 80 14 00 00 00  24 00 00 00 00 00 00 00  |........$.......|
00014510  34 00 00 00 01 02 00 00  00 00 00 05 20 00 00 00  |4........... ...|
00014520  20 02 00 00 01 02 00 00  00 00 00 05 20 00 00 00  | ........... ...|
00014530  20 02 00 00 02 00 1c 00  01 00 00 00 00 03 14 00  | ...............|
00014540  ff 01 1f 00 01 01 00 00  00 00 00 01 00 00 00 00  |................|
00014550  80 00 00 00 28 00 00 00  00 00 00 00 00 00 02 00  |....(...........|
00014560  0d 00 00 00 18 00 00 00  68 65 6c 6c 6f 20 77 6f  |........hello wo|
00014570  72 6c 64 21 0a 00 00 00  ff ff ff ff 00 00 00 00  |rld!............|
00014580  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000145f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 05 00  |................|
00014600  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000147f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 05 00  |................|
00014800  46 49 4c 45 30 00 03 00  00 00 00 00 00 00 00 00  |FILE0...........|
00014810  01 00 01 00 38 00 01 00  88 01 00 00 00 04 00 00  |....8...........|
00014820  00 00 00 00 00 00 00 00  04 00 00 00 42 00 00 00  |............B...|
00014830  05 00 00 00 00 00 00 00  10 00 00 00 48 00 00 00  |............H...|
00014840  00 00 00 00 00 00 00 00  30 00 00 00 18 00 00 00  |........0.......|
00014850  4a 90 e6 cc e8 3d ce 01  f8 a5 e6 cc e8 3d ce 01  |J....=.......=..|
00014860  f8 a5 e6 cc e8 3d ce 01  4a 90 e6 cc e8 3d ce 01  |.....=..J....=..|
00014870  20 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  | ...............|
00014880  30 00 00 00 70 00 00 00  00 00 00 00 00 00 03 00  |0...p...........|
00014890  56 00 00 00 18 00 01 00  05 00 00 00 00 00 05 00  |V...............|
000148a0  4a 90 e6 cc e8 3d ce 01  4a 90 e6 cc e8 3d ce 01  |J....=..J....=..|
*
000148c0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000148d0  20 00 00 00 00 00 00 00  0a 00 28 00 66 00 69 00  | .........(.f.i.|
000148e0  6c 00 65 00 30 00 2e 00  74 00 78 00 74 00 00 00  |l.e.0...t.x.t...|
000148f0  50 00 00 00 68 00 00 00  00 00 00 00 00 00 01 00  |P...h...........|
00014900  50 00 00 00 18 00 00 00  01 00 04 80 14 00 00 00  |P...............|
00014910  24 00 00 00 00 00 00 00  34 00 00 00 01 02 00 00  |$.......4.......|
00014920  00 00 00 05 20 00 00 00  20 02 00 00 01 02 00 00  |.... ... .......|
00014930  00 00 00 05 20 00 00 00  20 02 00 00 02 00 1c 00  |.... ... .......|
00014940  01 00 00 00 00 03 14 00  ff 01 1f 00 01 01 00 00  |................|
00014950  00 00 00 01 00 00 00 00  80 00 00 00 28 00 00 00  |............(...|
00014960  00 00 00 00 00 00 02 00  10 00 00 00 18 00 00 00  |................|
00014970  51 57 45 52 54 31 32 33  34 35 30 39 38 37 36 0a  |QWERT1234509876.|
00014980  ff ff ff ff 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00014990  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000149f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 05 00  |................|
00014a00  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00014bf0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 05 00  |................|
00014c00

同时确认了，当文件数据很少的时候，MFT中的数据属性直接将其数据存放下了。testDir下面的文件只有一个，所以它的索引根属性
AT_INDEX_ROOT直接将file1的索引给存放下了。从0x00014150开始对应的数据结构是ATTR_RECORD，从 0x00014170开始对应的数据结构是
typedef struct {
/*  0*/ ATTR_TYPES type;        /* Type of the indexed attribute. Is
                       $FILE_NAME for directories, zero
                       for view indexes. No other values
                       allowed. */
/*  4*/ COLLATION_RULES collation_rule; /* Collation rule used to sort the
                       index entries. If type is $FILE_NAME,
                       this must be COLLATION_FILE_NAME. */
/*  8*/ u32 index_block_size;       /* Size of index block in bytes (in
                       the index allocation attribute). */
/* 12*/ s8 clusters_per_index_block;    /* Size of index block in clusters (in
                       the index allocation attribute), when
                       an index block is >= than a cluster,
                       otherwise sectors per index block. */
/* 13*/ u8 reserved[3];         /* Reserved/align to 8-byte boundary. */
/* 16*/ INDEX_HEADER index;     /* Index header describing the
                       following index entries. */
/* sizeof()= 32 bytes */
} __attribute__((__packed__)) INDEX_ROOT;
其中
typedef struct {
/*  0*/ u32 entries_offset; /* Byte offset from the INDEX_HEADER to first
                   INDEX_ENTRY, aligned to 8-byte boundary.  */
/*  4*/ u32 index_length;   /* Data size in byte of the INDEX_ENTRY's,
                   including the INDEX_HEADER, aligned to 8. */
/*  8*/ u32 allocated_size; /* Allocated byte size of this index (block),
                   multiple of 8 bytes. See more below.      */
    /* 
       For the index root attribute, the above two numbers are always
       equal, as the attribute is resident and it is resized as needed.
       
       For the index allocation attribute, the attribute is not resident 
       and the allocated_size is equal to the index_block_size specified 
       by the corresponding INDEX_ROOT attribute minus the INDEX_BLOCK 
       size not counting the INDEX_HEADER part (i.e. minus -24).
     */
/* 12*/ INDEX_HEADER_FLAGS ih_flags;    /* Bit field of INDEX_HEADER_FLAGS.  */
/* 13*/ u8 reserved[3];         /* Reserved/align to 8-byte boundary.*/
/* sizeof() == 16 */
} __attribute__((__packed__)) INDEX_HEADER;
其中
typedef enum {
    /* When index header is in an index root attribute: */
    SMALL_INDEX = 0, /* The index is small enough to fit inside the
                index root attribute and there is no index
                allocation attribute present. */
    LARGE_INDEX = 1, /* The index is too large to fit in the index
                root attribute and/or an index allocation
                attribute is present. */
    /*
     * When index header is in an index block, i.e. is part of index
     * allocation attribute:
     */
    LEAF_NODE   = 0, /* This is a leaf node, i.e. there are no more
                nodes branching off it. */
    INDEX_NODE  = 1, /* This node indexes other nodes, i.e. is not a
                leaf node. */
    NODE_MASK   = 1, /* Mask for accessing the *_NODE bits. */
} __attribute__((__packed__)) INDEX_HEADER_FLAGS;
INDEX_HEADER.ih_flags==0x00==SMALL_INDEX， 对比前面的根目录的是LARGE_INDEX，顾名思义这里是小索引。
从 0x00014190 -  0x000141ef对应的数据结构是INDEX_ENTRY，从 0x000141f0 -  0x000141ff是个结束索引实体。除根目录外的目录下无本身的索引。

回望根目录的根索引，其
00005560                                           00 00 00 00 00 00 00 00  |(...............|
00005570  18 00 00 00 03 00 00 00  00 00 00 00 00 00 00 00  |................|
INDEX_ENTRY后面还跟了个VCN指向AT_INDEX_ALLOCATION类型的属性所在的相对簇号。


AT_BITMAP位图属性在不同的MFT中表意不同，在根目录记录项其值是 
000055f0  01 00 00 00 00 00 00 00
表示间接索引簇的使用情况，在MFT记录项，它表示MFT记录项的使用情况，当然记录项中放不下，只能是非常驻属性了。
00004180  08 00 00 00 00 00 00 00  11 01 02 00 00 00 00 00  |................|
可以看出其放在[2]号簇，使用了1簇的空间。当记录项超过4K*8时会使用2簇空间，当记录项的数量超过8K*8时会增加一个新的mapping pairs array，开辟新空间存放。


回望MFT本身的MFT记录项

[root@localhost ~]$ hexdump -C -n 1024 -s 16k /dev/sda1   
00004000  46 49 4c 45 30 00 03 00  00 00 00 00 00 00 00 00  |FILE0...........|
*略
00004030  02 00 00 00 00 00 00 00  10 00 00 00 60 00 00 00  |............`...|
*略
000041f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 02 00  |................|
00004200  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000043f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 02 00  |................|
00004400
其MFT_RECORD和ATTR_RECORD间有8字节的间隙，从0x00004030 - 0x00004037。这个偏移是由 MFT_RECORD.usa_ofs决定，一般是紧跟MFT_RECORD后面；
范围是由MFT_RECORD.usa_count决定，再经过8字节 对齐。
usa(update sequence array)是更新序列数组的意思，每个扇区(512B)有一个。因为此时 一条MFT记录项占1KB，所以这里有两个。
还有个usn(update sequence number)是更新序列序号，每使用 一次就加1，直到UINT16_MAX后再从1开始。
一个usn+两个usa成员 ==  MFT_RECORD.usa_count == 0x0003了。存放的顺序是usn在前面，这里的usn == 0x0002，两个usa成员都==0x0000，
是这样来的，从每扇区的最后两字节获取，也就是*000041fe和*000043fe，而 每扇区的最后两字节赋值为usn，所以其值都为0x0002。
具体的实现参考libntfs- 3g/mst.c:ntfs_mst_pre_write_fixup。


回望根目录的索引块
[root@localhost ~]$ hexdump -C root_index 
00000000  49 4e 44 58 28 00 09 00  00 00 00 00 00 00 00 00  |INDX(...........|
00000010  00 00 00 00 00 00 00 00  28 00 00 00 d0 04 00 00  |........(.......|
00000020  e8 0f 00 00 00 00 00 00  03 00 ce 01 00 00 00 00  |................|
00000030  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000040  04 00 00 00 00 00 04 00  68 00 52 00 00 00 00 00  |........h.R.....|

* 略

000001e0  05 00 00 00 00 00 05 00  80 26 b9 64 57 3c ce 01  |.........&.dW<..|
000001f0  80 26 b9 64 57 3c ce 01  80 26 b9 64 57 3c 03 00  |.&.dW<...&.dW<..|
00000200  80 26 b9 64 57 3c ce 01  00 00 00 00 00 00 00 00  |.&.dW<..........|

* 略

000003f0  80 26 b9 64 57 3c ce 01  00 00 02 00 00 00 03 00  |.&.dW<..........|
00000400  00 00 02 00 00 00 00 00  06 00 00 00 00 00 00 00  |................|
00000410  07 03 24 00 55 00 70 00  43 00 61 00 73 00 65 00  |..$.U.p.C.a.s.e.|
* 略
000005f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 03 00  |................|
00000600  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000007f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 03 00  |................|
00000800  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000009f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 03 00  |................|
00000a00  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00000bf0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 03 00  |................|
00000c00  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00000df0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 03 00  |................|
00000e00  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00000ff0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 03 00  |................|
00001000
其最前面对应的数据结构是
typedef struct {
/*  0   NTFS_RECORD; -- Unfolded here as gcc doesn't like unnamed structs. */
    NTFS_RECORD_TYPES magic;/* Magic is "INDX". */
    u16 usa_ofs;        /* See NTFS_RECORD definition. */
    u16 usa_count;      /* See NTFS_RECORD definition. */

/*  8*/ LSN lsn;        /* $LogFile sequence number of the last
                   modification of this index block. */
/* 16*/ VCN index_block_vcn;    /* Virtual cluster number of the index block. */
/* 24*/ INDEX_HEADER index; /* Describes the following index entries. */
/* sizeof()= 40 (0x28) bytes */
/*
 * When creating the index block, we place the update sequence array at this
 * offset, i.e. before we start with the index entries. This also makes sense,
 * otherwise we could run into problems with the update sequence array
 * containing in itself the last two bytes of a sector which would mean that
 * multi sector transfer protection wouldn't work. As you can't protect data
 * by overwriting it since you then can't get it back...
 * When reading use the data from the ntfs record header.
 */
} __attribute__((__packed__)) INDEX_BLOCK;
其中
typedef struct {
/*  0*/ u32 entries_offset; /* Byte offset from the INDEX_HEADER to first
                   INDEX_ENTRY, aligned to 8-byte boundary.  */
/*  4*/ u32 index_length;   /* Data size in byte of the INDEX_ENTRY's,
                   including the INDEX_HEADER, aligned to 8. */
/*  8*/ u32 allocated_size; /* Allocated byte size of this index (block),
                   multiple of 8 bytes. See more below.      */
    /* 
       For the index root attribute, the above two numbers are always
       equal, as the attribute is resident and it is resized as needed.
       
       For the index allocation attribute, the attribute is not resident 
       and the allocated_size is equal to the index_block_size specified 
       by the corresponding INDEX_ROOT attribute minus the INDEX_BLOCK 
       size not counting the INDEX_HEADER part (i.e. minus -24).
     */
/* 12*/ INDEX_HEADER_FLAGS ih_flags;    /* Bit field of INDEX_HEADER_FLAGS.  */
/* 13*/ u8 reserved[3];         /* Reserved/align to 8-byte boundary.*/
/* sizeof() == 16 */
} __attribute__((__packed__)) INDEX_HEADER;
和INDEX_ENTRY的间隙同样是由于usa造成的，因为此时一个索引块占4KB，所以有8个，INDEX_BLOCK.usa_count==0x0009，占用18Byte，再经过8字节对齐成24Byte了。
后面的值和前面MFT记录项中一样的替换方法。
这里的usn==0x0003，创建上面那些测试文件后
[root@localhost ~]$ hexdump -C -n 16 -s 32 root_index2  
00000020  e8 0f 00 00 00 00 00 00  0b 00 ce 01 00 00 00 00  |................|
usn==0x000b了。


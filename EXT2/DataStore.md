对应于Windows上文件系统的‘簇’，Linux上文件系统是将几个扇区组成‘块’来使用的。容量<512MB时，块大小为1KB，否则为4KB，当然可能会有些微调。

超级块：包含文件系统的描述信息，和FAT32不同的是它和引导代码分开了。
inode：全称就是index node，索引节点。

超级块是全局的，除块组0外，还会在后面某些块组有备份，这个好理解。需特别说明的是：
1、块组描述表(GDT)，一个块组只需一条块组描述，这里之所以是‘表’，是因为它也是全局的，所有块组的描述组成一个表放在一起，当然GDT也有多个备份。
2、Reserved GDT用于支持扩充的，是可选项，和GDT一样占用的是以块为单位的容量空间。
3、数据块位图，很容易和后面的“数据块”扯上联系，其实它描述的是本块组内的所有‘块’的位图，广义上的数据块是除引导块之外的所有‘块’的。
4、引导块的大小固定为1KB，不受文件系统划定的块大小的影响，其值应该是/usr/include/linux/fs.h:#define BLOCK_SIZE确定的。
5、超级块的大小由/usr/include/linux/ext2_fs.h里的
 /*
   * Structure of the super block
   */
  struct ext2_super_block {
      __le32  s_inodes_count;     /* Inodes count */
      __le32  s_blocks_count;     /* Blocks count */
      __le32  s_r_blocks_count;   /* Reserved blocks count */
      __le32  s_free_blocks_count;    /* Free blocks count */
      __le32  s_free_inodes_count;    /* Free inodes count */
      __le32  s_first_data_block; /* First Data Block */
      __le32  s_log_block_size;   /* Block size */
      __le32  s_log_frag_size;    /* Fragment size */
      __le32  s_blocks_per_group; /* # Blocks per group */
      __le32  s_frags_per_group;  /* # Fragments per group */
      __le32  s_inodes_per_group; /* # Inodes per group */
      __le32  s_mtime;        /* Mount time */
      __le32  s_wtime;        /* Write time */
      __le16  s_mnt_count;        /* Mount count */
      __le16  s_max_mnt_count;    /* Maximal mount count */
      __le16  s_magic;        /* Magic signature */
      __le16  s_state;        /* File system state */
      __le16  s_errors;       /* Behaviour when detecting errors */
      __le16  s_minor_rev_level;  /* minor revision level */
      __le32  s_lastcheck;        /* time of last check */
      __le32  s_checkinterval;    /* max. time between checks */
      __le32  s_creator_os;       /* OS */
      __le32  s_rev_level;        /* Revision level */
      __le16  s_def_resuid;       /* Default uid for reserved blocks */
      __le16  s_def_resgid;       /* Default gid for reserved blocks */
      /*
       * These fields are for EXT2_DYNAMIC_REV superblocks only.
       *
       * Note: the difference between the compatible feature set and
       * the incompatible feature set is that if there is a bit set
       * in the incompatible feature set that the kernel doesn't
       * know about, it should refuse to mount the filesystem.
       * 
       * e2fsck's requirements are more strict; if it doesn't know
       * about a feature in either the compatible or incompatible
       * feature set, it must abort and not try to meddle with
       * things it doesn't understand...
       */
      __le32  s_first_ino;        /* First non-reserved inode */
      __le16   s_inode_size;      /* size of inode structure */
      __le16  s_block_group_nr;   /* block group # of this superblock */
      __le32  s_feature_compat;   /* compatible feature set */
      __le32  s_feature_incompat;     /* incompatible feature set */
      __le32  s_feature_ro_compat;    /* readonly-compatible feature set */
      __u8    s_uuid[16];     /* 128-bit uuid for volume */
      char    s_volume_name[16];  /* volume name */
      char    s_last_mounted[64];     /* directory where last mounted */
      __le32  s_algorithm_usage_bitmap; /* For compression */
      /*
       * Performance hints.  Directory preallocation should only
       * happen if the EXT2_COMPAT_PREALLOC flag is on.
       */
      __u8    s_prealloc_blocks;  /* Nr of blocks to try to preallocate*/
      __u8    s_prealloc_dir_blocks;  /* Nr to preallocate for dirs */
      __u16   s_padding1;
      /*
       * Journaling support valid if EXT3_FEATURE_COMPAT_HAS_JOURNAL set.
       */
      __u8    s_journal_uuid[16]; /* uuid of journal superblock */
      __u32   s_journal_inum;     /* inode number of journal file */
      __u32   s_journal_dev;      /* device number of journal file */
      __u32   s_last_orphan;      /* start of list of inodes to delete */
      __u32   s_hash_seed[4];     /* HTREE hash seed */
      __u8    s_def_hash_version; /* Default hash version to use */
      __u8    s_reserved_char_pad;
      __u16   s_reserved_word_pad;
      __le32  s_default_mount_opts;
      __le32  s_first_meta_bg;    /* First metablock block group */
      __u32   s_reserved[190];    /* Padding to the end of the block */
  };
这个数据额结构决定，刚好1KB，而且有很宽的保留字节，足够保证后续ext3等扩充。超级块是在块大小划定后才放到某些块组的最前面块上的，
所以定义了能划定的最小块大小#define EXT2_MIN_BLOCK_SIZE 1024 以保证能放下超级块。此外，块的大小必须是2的乘方，所以必然是1KB的整数倍。
如果大于1KB，则最少是2KB，足够放下引导块+超级块，这时块组0的超级块和前面的引导块共用第[0] 块，也就导致了“数据块”从[0]开始，
因而有sb->s_first_data_block = (block_size& nbsp;> EXT2_MIN_BLOCK_SIZE) ? 0 : 1;这种奇怪的正确逻 辑。

前面根据分区容量，确定块大小后，相除就可以确定块的个数了。每个块组里面的数据块位图，只能占用1块，而位图是用1bit的0/1分别表示块是否被占用，
所以块组内最多有“块大小 * 8(Byte/bit)”个块，也就是块组容量上限时“块大 小 * 8 * 块大小”。若块大小为4KB，那么块组容量为128MB。
分区容量/块组容量就可以确定块组个数了。每个块组有一个条组描述
/*
 * Structure of a blocks group descriptor
 */
struct ext2_group_desc
{
    __le32  bg_block_bitmap;        /* Blocks bitmap block */
    __le32  bg_inode_bitmap;        /* Inodes bitmap block */
    __le32  bg_inode_table;     /* Inodes table block */
    __le16  bg_free_blocks_count;   /* Free blocks count */
    __le16  bg_free_inodes_count;   /* Free inodes count */
    __le16  bg_used_dirs_count; /* Directories count */
    __le16  bg_pad;
    __le32  bg_reserved[3];
};
像bg_block_bitmap指向本块组内数据块位图的块号，是全局绝对地址，而不是相对于本块组头的地址，其它几个成员类似。
根据组描述数据结构大小、块组个数、块的大小，就可以计算出GDT占用多少块了。
索引节点(inode)表：表示本块组内的无疑是结构体

  /*
   * Structure of an inode on the disk
   */
  struct ext2_inode {
      __le16  i_mode;     /* File mode */
      __le16  i_uid;      /* Low 16 bits of Owner Uid */
      __le32  i_size;     /* Size in bytes */
      __le32  i_atime;    /* Access time */
      __le32  i_ctime;    /* Creation time */
      __le32  i_mtime;    /* Modification time */
      __le32  i_dtime;    /* Deletion Time */
      __le16  i_gid;      /* Low 16 bits of Group Id */
      __le16  i_links_count;  /* Links count */
      __le32  i_blocks;   /* Blocks count */
      __le32  i_flags;    /* File flags */
      union {
          struct {
              __le32  l_i_reserved1;
          } linux1;
          struct {
              __le32  h_i_translator;
          } hurd1;
          struct {
              __le32  m_i_reserved1;
          } masix1;
      } osd1;             /* OS dependent 1 */
      __le32  i_block[EXT2_N_BLOCKS];/* Pointers to blocks */
      __le32  i_generation;   /* File version (for NFS) */
      __le32  i_file_acl; /* File ACL */
      __le32  i_dir_acl;  /* Directory ACL */
      __le32  i_faddr;    /* Fragment address */
      union {
          struct {
              __u8    l_i_frag;   /* Fragment number */
              __u8    l_i_fsize;  /* Fragment size */
              __u16   i_pad1;
              __le16  l_i_uid_high;   /* these 2 fields    */
              __le16  l_i_gid_high;   /* were reserved2[0] */
              __u32   l_i_reserved2;
          } linux2;
          struct {
              __u8    h_i_frag;   /* Fragment number */
              __u8    h_i_fsize;  /* Fragment size */
              __le16  h_i_mode_high;
              __le16  h_i_uid_high;
              __le16  h_i_gid_high;
              __le32  h_i_author;
          } hurd2;
          struct {
              __u8    m_i_frag;   /* Fragment number */
              __u8    m_i_fsize;  /* Fragment size */
              __u16   m_pad1;
              __u32   m_i_reserved2[2];
          } masix2;
      } osd2;             /* OS dependent 2 */
  };
的表，但是它的所索引的数据块i_block并不限定只能是本块组内的，而是全局的。i_block可以是三级间接索引，能表示的容量是[1块，到很大]，
所以整个分区需要多少个inode视存放的文件大小而定。默认16KB使用一个inode，据此计算出inode的个数，然后平摊到各个块组，
再根据 inode本身的大小，计算出inode表所占的块。需要注意的是，inode的间接i_block是征用普通的数据块放inode，所以不用担心索引不够用。
此外，分区容量>=512MB时分配2 * sizeof (struct  ext2_inode)给inode做扩充用。

索引节点表占用的块确定后，就可以确定
数据块位图：“数据块”之前的那些已用的块对应的bit位标记为1，若最后一块组是前面整除的余数，也就是块组内块数不足，后面用不上的bit位也标记为1。
如果扫描坏道的话，坏块也会标记为1。
索引节点位图：占用1块的空间，表示inode表中inode的使用情况，inode数量肯定比组块少，所以放得下。用不上的bit也会标记为1，一般块大小4KB，
而16KB使用一个inode，所以相对于前面的数据块位图，索引节点位图后面的3/4会用不上。

遍历确定各块组的上述数据过程中，各块组的组描述随即确定，最终的GDT也就确定了。遍历过程中，将各块组的gd[i].bg_free_blocks_count累加
起来就是超级块的sb->s_free_blocks_count。

ext2文件系统不只有元数据，还有元文件，也就是根目录下有
/.    /..    /lost+found    /lost+found/.    /lost+found/..
这些文件/文件夹，在格式化过程就必需填充这些文件/文件夹索引和数据。

ext2文件系统的文件夹/目录，也是作为一个特殊的文件，只不过它的内容是其下文件/文件夹的索引，数据结构如下：
/*
 * The new version of the directory entry.  Since EXT2 structures are
 * stored in intel byte order, and the name_len field could never be
 * bigger than 255 chars, it's safe to reclaim the extra byte for the
 * file_type field.
 */
struct ext2_dir_entry_2 {
    __le32  inode;          /* Inode number */
    __le16  rec_len;        /* Directory entry length */
    __u8    name_len;       /* Name length */
    __u8    file_type;
    char    name[EXT2_NAME_LEN];    /* File name */
};
所以，文件查找是通过文件名目录层层找到文件的inode号，根据inode号计算出inode地址找到inode，然后根据inode.i_block[]能找到它所有的数据块，是B-数树算法。

所以，创建元文件时需要设置好其inode号，inode值，还需要写好目录项。元文件放在块组[0]，会影响到块组[0]上的那些元数据，
及超级块 /GDT等全局元数据，需要更新。也可以在前面就预算好，参考：e2fsprogs-1.42.5/misc/mke2fs.c


根据inode号计算inode地址的程式是：
__le32 group_num =& nbsp;inode_num / sb->s_inodes_per_group;         // 计算索引节点所在的块组，注意inode号是从1开始计数
struct ext2_inode *pi = (struct ext2_inode *)(gd[group_num].bg_inode_table * EXT2_BLOCK_SIZE(sb) + ((inode_num - 1) % inode_per_group) * sb->s_inode_size);

通过ext2_inode.i_block[EXT2_N_BLOCKS];找到文件的内容/数据，在头文件中定义了
/*
 * Constants relative to the data blocks
 */
#define EXT2_NDIR_BLOCKS        12
#define EXT2_IND_BLOCK          EXT2_NDIR_BLOCKS
#define EXT2_DIND_BLOCK         (EXT2_IND_BLOCK + 1)
#define EXT2_TIND_BLOCK         (EXT2_DIND_BLOCK + 1)
#define EXT2_N_BLOCKS           (EXT2_TIND_BLOCK + 1)
也就是.i_block[EXT2_IND_BLOCK]开始是间接索引，先索引到一个数据块，该数据块上放的不是文件的内容数据块，而是指向其它数据块的指针。
同理还有二级、三级间接索引，假设指针的类型大小sizeof (ptrdiff_t)==4，那么寻块方式如下图： 

 假设块大小为4KB，那么文件大小超过12 * 4KB == 48KB就需使用一级间接指针，超过 ((4KB/4) + 12) * 4KB == 4144KB就需使用二级间接指针，
超过(pow(4KB/4, 2) + (4KB /4) + 12) * 4KB == 1049648K就需要使用三级间接指针。


下面进行实验
[root@localhost ~]$ ./mke2fs -t ext2 /dev/sda1
mke2fs 1.42.5 (29-Jul-2012)
Filesystem label=
OS type: Linux
Block size=4096 (log=2)
Fragment size=4096 (log=2)
Stride=0 blocks, Stripe width=0 blocks
30203904 inodes, 120785670 blocks
6039283 blocks (5.00%) reserved for the super user
First data block=0
Maximum filesystem blocks=0
3687 block groups
32768 blocks per group, 32768 fragments per group
8192 inodes per group
Superblock backups stored on blocks: 
        32768, 98304, 163840, 229376, 294912, 819200, 884736, 1605632, 2654208, 
        4096000, 7962624, 11239424, 20480000, 23887872, 71663616, 78675968, 
        102400000

Allocating group tables: done                            
Writing inode tables: done                            
Writing superblocks and filesystem accounting information: done   
[root@localhost ~]$ mount /dev/sda1 /var/
[root@localhost ~]$ mkdir /var/testdir
[root@localhost ~]$ i=0;while [ $i -lt 8194 ]; do echo file $i data > /var/testdir/$i.txt; ((++i)); done
[root@localhost ~]$ ls -i /
      2 var 
根目录索引节点号是
#define EXT2_ROOT_INO        2  /* Root inode */与之对应
套上面的程式:group_num==0
转储打印GDT第一块。
[root@localhost ~]$ hexdump -C -n 32 -s 4k /dev/sda1 
00000000  01 04 00 00 02 04 00 00  03 04 00 00 f7 79 f5 1f  |.............y..|
00000010  02 00 04 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000020
可知gd[0].bg_inode_table == 0x00000403, 套上面的程式
[root@localhost ~]$ hexdump -C -n 512 -s 4108k /dev/sda1   
00000000  00 00 00 00 00 00 00 00  2b b9 4d 51 2b b9 4d 51  |........+.MQ+.MQ|
00000010  2b b9 4d 51 00 00 00 00  00 00 00 00 00 00 00 00  |+.MQ............|
00000020  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00000100  ed 41 00 00 00 10 00 00  a7 b9 4d 51 a2 b9 4d 51  |.A........MQ..MQ|
00000110  a2 b9 4d 51 00 00 00 00  00 00 04 00 08 00 00 00  |..MQ............|
00000120  00 00 00 00 00 00 00 00  03 06 00 00 00 00 00 00  |................|
00000130  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00000180  1c 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000190  2b b9 4d 51 00 00 00 00  00 00 00 00 00 00 00 00  |+.MQ............|
000001a0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
上面inode[1]就是根目录的索引节点，inode[1].i_block[0] == 0x00000603是它数据/内容的第一块
[root@localhost ~]$ hexdump -C -n 512 -s 6156k /dev/sda1
00000000  02 00 00 00 0c 00 01 02  2e 00 00 00 02 00 00 00  |................|
00000010  0c 00 02 02 2e 2e 00 00  0b 00 00 00 14 00 0a 02  |................|
00000020  6c 6f 73 74 2b 66 6f 75  6e 64 00 00 01 e0 8a 01  |lost+found......|
00000030  d4 0f 07 02 74 65 73 74  64 69 72 00 00 00 00 00  |....testdir.....|
00000040  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
找到testdir在根目录下的inode号是0x018ae001，和
[root@localhost ~]$ ls -i /var/testdir 
25878529 testdir         11 lost+found
出来的相等。

继续套公式，计算出testdir的inode号对应的，group_num==3159
相对GDT开头的3159 * 32位置，加上前面的超级块(和引导块同块)，最终为。
[root@localhost ~]$ hexdump -C -n 32 -s 105184 /dev/sda1
00000000  00 80 2b 06 01 80 2b 06  02 80 2b 06 dd 5d 00 00  |..+...+...+..]..|
00000010  01 00 04 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
可知gd[3159].bg_inode_table == 0x062b8002，套上面的程式
[root@localhost ~]$ hexdump -C -n 256 -s 423993810944 /dev/sda1                                  
00000000  ed 41 00 00 00 10 02 00  15 ba 4d 51 ea b9 4d 51  |.A........MQ..MQ|
00000010  ea b9 4d 51 00 00 00 00  00 00 02 00 10 01 00 00  |..MQ............|
00000020  00 00 00 00 00 00 00 00  00 b8 2b 06 01 b8 2b 06  |..........+...+.|
00000030  02 b8 2b 06 03 b8 2b 06  04 b8 2b 06 05 b8 2b 06  |..+...+...+...+.|
00000040  06 b8 2b 06 07 b8 2b 06  08 b8 2b 06 09 b8 2b 06  |..+...+...+...+.|
00000050  0a b8 2b 06 0b b8 2b 06  0c b8 2b 06 00 00 00 00  |..+...+...+.....|
00000060  00 00 00 00 c7 02 13 c7  00 00 00 00 00 00 00 00  |................|
00000070  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
这里inode.i_block[0] == 0x062bb800是它数据/内容的第一块
[root@localhost ~]$ hexdump -C -n 256 -s 414113792k /dev/sda1 
00000000  01 e0 8a 01 0c 00 01 02  2e 00 00 00 02 00 00 00  |................|
00000010  0c 00 02 02 2e 2e 00 00  02 e0 8a 01 10 00 05 01  |................|
00000020  30 2e 74 78 74 00 00 00  03 e0 8a 01 10 00 05 01  |0.txt...........|
00000030  31 2e 74 78 74 00 00 00  04 e0 8a 01 10 00 06 01  |1.txt...........|
00000040  31 30 2e 74 78 74 00 00  05 e0 8a 01 10 00 07 01  |10.txt..........|
00000050  31 30 30 2e 74 78 74 00  06 e0 8a 01 10 00 08 01  |100.txt.........|
00000060  31 30 30 30 2e 74 78 74  07 e0 8a 01 10 00 08 01  |1000.txt........|
00000070  31 30 30 31 2e 74 78 74  08 e0 8a 01 10 00 08 01  |1001.txt........|
00000080  31 30 30 32 2e 74 78 74  09 e0 8a 01 10 00 08 01  |1002.txt........|
00000090  31 30 30 33 2e 74 78 74  0a e0 8a 01 10 00 08 01  |1003.txt........|
000000a0  31 30 30 34 2e 74 78 74  0b e0 8a 01 10 00 08 01  |1004.txt........|
000000b0  31 30 30 35 2e 74 78 74  0c e0 8a 01 10 00 08 01  |1005.txt........|
000000c0  31 30 30 36 2e 74 78 74  0d e0 8a 01 10 00 08 01  |1006.txt........|
000000d0  31 30 30 37 2e 74 78 74  0e e0 8a 01 10 00 08 01  |1007.txt........|
000000e0  31 30 30 38 2e 74 78 74  0f e0 8a 01 10 00 08 01  |1008.txt........|
000000f0  31 30 30 39 2e 74 78 74  10 e0 8a 01 10 00 07 01  |1009.txt........|
是该testdir/下面文件的索引号、文件名等，数据正如预期。 


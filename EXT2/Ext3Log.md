对应于Windows上文件系统的‘簇’，Linux上文件系统是将几个扇区组成‘块’来使用的。容量<512MB时，块大小为1KB，否则为4KB，当然可能会有些微调。

超级块：包含文件系统的描述信息，和FAT32不同的是它和引导代码分开了。
inode：全称就是index node，索引节点。

假设一个大于4GB的硬盘，块大小是4KB，块组是128MB，根据下面函数可以算出
/*
 * Find a reasonable journal file size (in blocks) given the number of blocks
 * in the filesystem.  For very small filesystems, it is not reasonable to
 * have a journal that fills more than half of the filesystem.
 */
int ext2fs_default_journal_size(__u64 num_blocks)
{
    if (num_blocks < 2048)
        return 0;
    if (num_blocks < 32768)
        return (1024);
    if (num_blocks < 256*1024)
        return (4096);
    if (num_blocks < 512*1024)
        return (8192);
    if (num_blocks < 1024*1024)
        return (16384);
    return 32768;
}
日志文件数据有32KB块，也就是128MB，一个块组的bg_free_blocks_count肯定不够，所以会跨块组。还有，这个大小的文件需要使用二级间接索引。

ext2_inode. i_block[EXT2_N_BLOCKS];的寻块模式，如果数据块是连续的，则如下图：

这里的块是按需分配的，也就是不会用完能索引到的所有块，按文件大小需要多少块就分多少块，间接索引的指针也是能用掉多少就分配多少。
实现过程大致如下：
1、计算需要消耗多少块 = 文件大小消耗的块+被征用放间接索引的块
2、分配数据块，一般日志文件是从分区中间块组开始的，也就是块组[n/2]，精确的是块组内狭义数据块的开始，若为首块还要跳过已有的文件
jnl_inode->i_block[0] =& nbsp;gd[n/2].bg_inode_table + inode_table_blocks +  ((n/2 == 0) ? (1 + lost_and_found_blocks) : 0);开始到下一个块组，
uint32_t next_grp_start_block = sb->s_first_data_block +  (n/2 + 1) * sb->s_blocks_per_group;时，
跳到下一个块组的gd[n /2].bg_inode_table + inode_table_blocks，直到放置完数据。
如果要保护分区上其它已有数据，那么要根据块位图筛选空闲块，这样可能绵延3个或以上块组了。
3、将分配的数据块清空，将用来征用放间接索引的数据块，根据每个指针指向的地址，写入指针的值。
4、写日志的首块数据，也就是i_block[0]指向的块，数据内容参考e2fsprogs-1.42.5/lib/ext2fs/mkjournal.c:ext2fs_create_journal_superblock实现
/*
 * This function automatically sets up the journal superblock and
 * returns it as an allocated block.
 */
int ext2fs_create_journal_superblock(__u32 num_blocks, char  **ret_jsb)
{
    int block_size = 1 << (sb->s_log_block_size + EXT2_MIN_BLOCK_LOG_SIZE);
    journal_superblock_t *jsb = (journal_superblock_t *)malloc(sizeof (journal_superblock_t));
    memset (jsb, 0, sizeof (journal_superblock_t));

    jsb->s_header.h_magic = htonl(JFS_MAGIC_NUMBER);
    jsb->s_header.h_blocktype = htonl(JFS_SUPERBLOCK_V2);
    jsb->s_blocksize = htonl(block_size);
    jsb->s_maxlen = htonl(num_blocks);
    jsb->s_nr_users = htonl(1);
    jsb->s_first = htonl(1);
    jsb->s_sequence = htonl(1);
    memcpy(jsb->s_uuid, sb->s_uuid, sizeof(sb->s_uuid));

    *ret_jsb = (char *) jsb;
    return 0;
}
5、日志索引节点号sb->s_journal_inum，值一般设置为8，据此号将日志索引写到块组0的索引节点表。
6、超级块的sb->s_feature_compat |= EXT3_FEATURE_COMPAT_HAS_JOURNAL;还有其它项ext2时缺省，ext3时需要填上。
7、日志文件是根据sb->s_journal_inum定位的，不需要将它的文件名等记录写到根目录下。
具体实现参考：e2fsprogs-1.42.5/lib/ext2fs/mkjournal.c:ext2fs_add_journal_inode


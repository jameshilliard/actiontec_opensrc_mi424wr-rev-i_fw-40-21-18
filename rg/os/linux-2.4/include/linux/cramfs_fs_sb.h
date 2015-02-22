#ifndef _CRAMFS_FS_SB
#define _CRAMFS_FS_SB

/*
 * cramfs super-block data in memory
 */
struct cramfs_sb_info {
			unsigned long magic;
			unsigned long size;
			unsigned long blocks;
			unsigned long files;
			unsigned long flags;
			unsigned long sub_type;
			void *uncomp_buffer;
			int uncomp_blk_offset;
			int uncomp_blk_data_size;
};

#endif

#include <common.h>
#include <command.h>

/* flash.h */

#define FLASH_SECTION_MAGIC 0xFEEDBABE
#define MAX_SECTION_NAME 128
#define MAX_FILE_NAME 128

/* Possible logical types for a section */
typedef enum flash_section_type_t{
    FLASH_SECT_UNDEF = -1,
    FLASH_SECT_BOOT = 0,
    FLASH_SECT_FACTORY = 1,
    FLASH_SECT_IMAGE = 2,
    FLASH_SECT_CONF = 3,
    FLASH_SECT_BOOTCONF = 4,
    FLASH_SECT_USER = 5,
    FLASH_SECT_BACKUP_CONF = 6,
    FLASH_SECT_FLASH_LAYOUT = 7,
    FLASH_SECT_LOG = 8,	/* ACTION_TEC_PERSISTENT_LOG */
    FLASH_SECT_JVM = 9,	/* ACTION_TEC_JVM*/
    FLASH_SECT_OSGI = 10,/* ACTION_TEC_OSGI*/
} flash_section_type_t;

/* Possible struct types for a section */
typedef enum {
    FLASH_SEC_STRUCT_DEFAULT = 0,
} flash_section_struct_t;

typedef struct {
    char filename[MAX_FILE_NAME];
} flash_section_default_t;

/* Possible section's flags */
#define FLASH_SECT_WRITE_WARNING 0x0001
#define FLASH_SECT_NO_HEADER 0x0002
/* The header is at the end of the section */
#define FLASH_SECT_BACK_HEADER 0x0004
#define FLASH_SECT_VENDOR_HEADER 0x0008
#define FLASH_SECT_NO_ALWAYS_SYNC 0x0010
#define FLASH_SECT_UNLINK_ON_WRITE 0x0020

/*
 * magic - signature that means that data/header is present in the section.
 * size - size of data in the section
 * chksum - check sum of the data in the section + this struct
 *          (not including this crc field)
 * counter - the section with the largest counter is the active (current) one.
 * name - section name (free text, null terminated)
 */
typedef struct {
    u32 magic; 
    u32 size; 
    u32 chksum; 
    u32 counter;
    u32 start_offset;
    u8 name[MAX_SECTION_NAME];
} flash_section_header_t;

typedef struct flash_section_layout_t {
    flash_section_type_t type;
    u32 flags;
    u32 size;
    u32 offset;
    struct {
	flash_section_struct_t type;
	union {
	    /* Union size is fixed to 256, you must NOT exceed this size or
	     * dynamic flash layout will not work correctly */
	    char unsused[256];
	    flash_section_default_t def;
	} u;
    } s;
} flash_section_layout_t;

#if __BYTE_ORDER == __LITTLE_ENDIAN

static inline unsigned int swap32(unsigned int x)
{
    return (((x & 0xff000000) >> 24) | ((x & 0x00ff0000) >>  8) |
	((x & 0x0000ff00) <<  8) | ((x & 0x000000ff) << 24));
}

static inline unsigned short swap16(unsigned short x)
{
    return ((x & 0x0000ff00) >>  8) | ((x & 0x000000ff) << 8);
}

#define flash_swap_32(x)	swap32(x)
#define flash_swap_16(x)        swap16(x)

#else

#define flash_swap_32(x)	(x)
#define flash_swap_16(x)        (x)

#endif

#define SECT_OFFSET(idx) (flash_layout_get()[idx].offset)

#define SECT_TYPE(idx) (flash_layout_get()[idx].type)

#define SECT_SIZE(idx) (flash_layout_get()[idx].size)

#define HEADER_SIZE(idx) (HAS_HEADER(idx) ? \
    (int)sizeof(flash_section_header_t) : 0)

/* modified macros to work with section idx, not flash layout */
#define HAS_FLAG(idx, flag) (flash_layout_get()[idx].flags & (flag))

#define HAS_HEADER(idx) (!HAS_FLAG(idx, FLASH_SECT_NO_HEADER))

#define BACK_HEADER(idx) HAS_FLAG(idx, FLASH_SECT_BACK_HEADER)

#define VENDOR_HEADER(idx) HAS_FLAG(idx, FLASH_SECT_VENDOR_HEADER)					  
#define DATA_SIZE(idx) (SECT_SIZE(idx) - HEADER_SIZE(idx))

#define DATA_OFFSET(idx) (SECT_OFFSET(idx) + \
    (BACK_HEADER(idx) || VENDOR_HEADER(idx) ? 0 : HEADER_SIZE(idx)))

#define HEADER_OFFSET(idx) (HAS_HEADER(idx) ? \
    SECT_OFFSET(idx) + (BACK_HEADER(idx) ? DATA_SIZE(idx) : 0) \
    : 0)

/* vendor/c/flash_layout.c */
#define CONFIG_RG_FFS_MNT_DIR "/mnt/jffs"
#include "flash_layout.c"

/* permst.c, modified */

flash_section_layout_t *flash_layout_get(void)
{
	return flash_layout;
}

int flash_layout_section_count(void)
{
    return flash_section_count;
}

void chksum_update(u32 *sum, u8 *buf, u32 size)
{
    register u32 chksum = 0;
    register u32 tmp;

    /* The buffer may begin at an unaligned address, so process the unaligned
     * part one byte at a time */
    while (((u32)buf & 3) && size)
    {
	chksum += *(buf++);
	size--;
    }

    /* By the time we're here, 'buf' is 32-bit aligned so we can start reading 
     * from memory 32-bits at a time, thus minimizing access to memory */
    while(size >= 4)
    {
	tmp = *((u32 *)buf);
	chksum += tmp & 0xff;
	tmp >>= 8;
	chksum += tmp & 0xff;
	tmp >>= 8;
	chksum += tmp & 0xff;
	tmp >>= 8;
	chksum += tmp & 0xff;
	
	size -= 4;
	buf += 4;
    }

    /* Process unaligned leftovers one byte at a time */
    while(size--)
	chksum += *(buf++);

    *sum += chksum;
}

static void flash_swap_header(flash_section_header_t *header)
{
    header->magic = flash_swap_32(header->magic);
    header->size = flash_swap_32(header->size);
    header->chksum = flash_swap_32(header->chksum);
    header->counter = flash_swap_32(header->counter);
    header->start_offset = flash_swap_32(header->start_offset);
}

static int check_section(int sec, flash_section_header_t *header)
{
    u32 section_chksum = 0, chksum = 0;

    if (!HAS_HEADER(sec))
	return 0;

    *header = *(flash_section_header_t *)HEADER_OFFSET(sec);

    flash_swap_header(header);

    if (header->magic != FLASH_SECTION_MAGIC || header->size > DATA_SIZE(sec))
	return -1;

    /* Update check sum for the header itself */
    section_chksum = header->chksum;
    header->chksum = 0;
    chksum_update(&chksum, (u8 *)header, sizeof(*header));
    /* Update check sum for the data in section */
    chksum_update(&chksum, (u8 *)DATA_OFFSET(sec), header->size);

    header->chksum = section_chksum;

    return (chksum == section_chksum) ? 0 : -1;
}

static int find_section(void)
{
    int section = 0, size = 0, counter = -1, i;
    flash_section_header_t header;
    long long t = get_ticks();

    printf("Looking for active section/image:\n");
    
    for (i = 0; i < flash_layout_section_count(); i++)
    {
	printf("%d. section: type:%d ", i, SECT_TYPE(i));

	if (SECT_TYPE(i) != FLASH_SECT_IMAGE)
	{
	    printf("not an image\n");
	    continue;
	}

	if (!SECT_OFFSET(i))
	{
	    printf("uninitialized offset\n");
	    continue;
	}

	printf("checking at address 0x%x... ", SECT_OFFSET(i));

	if (check_section(i, &header))
	{
	    printf("corrupted\n");
	    continue;
	}

	printf("ok: '%s' %#x@%#x count:%#x\n", header.name,
	    header.size, DATA_OFFSET(i), header.counter);
	
	if (counter == -1 || counter < header.counter)
	{
	    counter = header.counter;
	    size = header.size;
	    section = i;
	}
    }

    t = ((get_ticks() - t) * 1000) / get_tbclk();

    if (counter == -1)
    {
    	printf("No active section/image found!!! (%lld ms)\n", t);
    	return -1;
    }

    printf("Active section/image: %d count:%#x. (%lld ms)\n", section, counter, t);

    {
	char buf[32];
	sprintf(buf, "%#x", DATA_OFFSET(section));
	setenv("openrg_start", buf);
	sprintf(buf, "%#x", size);
	setenv("openrg_size", buf);
    }

    return section;
}

static int do_dualimage(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    int i;
    int sec;

    if (argc < 2)
    {
	cmd_usage(cmdtp);
	return 1;
    }

    printf("Updating image section offsets with passed parameters\n");
    sec = 0;
    for (i = 1; i < argc; i++)
    {
	unsigned long start = simple_strtoul(argv[i], NULL, 16);
	int sec_count = flash_layout_section_count();

	for (; sec < sec_count && SECT_TYPE(sec) != FLASH_SECT_IMAGE; sec++) ;

	if (sec == sec_count)
	    break;

	printf("Section %d at address 0x%lx\n", sec, start);
	SECT_OFFSET(sec) = start;
	sec++;
    }
    
    return find_section() > 0 ? 0 : -1;
}

U_BOOT_CMD(
	dualimage, 4, 0, do_dualimage,
	"sets openrg_start and openrg_size according to "
	"the current active image.\n",
	"addr#1 [addr#2 ...]"
);


#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <util/util.h>
#include <util/lines.h>
#include <util/sys.h>
#include <util/alloc.h>
#include <util/file.h>
#include <util/file_util.h>
#include <util/str.h>
#include <util/str_regex.h>
#include <util/sed.h>
#include <unistd.h>

static FILE *report_fp, *disk_img_fp;
static char *disk_image;
static char *STRIP = "strip";
static char *OBJCOPY = "objcopy";
static char *ARCHIVER = "gzip -f9 -c";
static char **disk_img_lines;
static char **summary_lines = NULL;
static char **sorted_lines = NULL;
static int is_cramfs_in_flash = cCONFIG_RG_CRAMFS_IN_FLASH;

/* detection of file types and calculation of sizes */

#define MOUNT_NONE         0
#define MOUNT_CRAMFS       0x01
#define MOUNT_MODFS        0x02
#define MOUNT_RAMDISK      0x04
#define MOUNT_LINUX        0x08
#define MOUNT_MOD_STAT     0x10

#define SECTION_NONE	   0
#define SECTION_MODULE     0x02
#define SECTION_LIB        0x04
#define SECTION_EXEC       0x08
#define SECTION_IMAGE      0x10
#define SECTION_FILE       0x20
#define SECTION_OTHER      0x40

#define SECTION_ALL	   0xff

#define TYPE_REGULAR_FILE  'f'
#define TYPE_SYMLINK	   'l'

#define LINUX_DIR_ABS "os/linux/"
#define LINUX_DIR "../../" LINUX_DIR_ABS

typedef struct mount_t {
    int mount;
    char *name;
    char *dir;
    char *img;
    int sections;
    char filetype; 
} mount_t;

mount_t mount_list[] = {
    { MOUNT_CRAMFS, "cramfs", "cramfs_dir", "cramfs.img", ~SECTION_OTHER,
        TYPE_REGULAR_FILE },
    { MOUNT_MODFS, "modfs", "modfs_dir", "mod.img", ~SECTION_OTHER,
        TYPE_REGULAR_FILE },
    { MOUNT_RAMDISK, "ramdisk", "ramdisk_dir", "rd.img", SECTION_ALL,
	TYPE_REGULAR_FILE},
    { MOUNT_LINUX, "linux", LINUX_DIR "vmlinux", LINUX_DIR "vmlinux",
	SECTION_OTHER, TYPE_REGULAR_FILE},
    /* MOUNT_MOD_STAT is usually counted as a part of MOUNT_LINUX */
    { 0, NULL }
};

typedef struct section_t {
    int section;
    char *name;
} section_t;

section_t section_list[] = {
    { SECTION_MODULE, "module" },
    { SECTION_LIB, "shared library" },
    { SECTION_EXEC, "executable" },
    { SECTION_IMAGE, "image" },
    { SECTION_FILE, "file" },
    { SECTION_OTHER, "other" },
    { 0, NULL }
};

static mount_t *get_mount(char *name)
{
    mount_t *mount;
    for (mount = mount_list; mount->mount; mount++)
    {
	if (!strcmp(mount->name, name))
	    return mount;
    }
    rg_error(LEXIT, "Invalid mount %s", name);
    return NULL;
}

typedef struct mem_t {
    int flash_uncomp;
    int flash;
    int ram;
    int slab;
} mem_t;

static void add_mem(mem_t *sum, mem_t *add)
{
    sum->flash_uncomp += add->flash_uncomp;
    sum->flash += add->flash;
    sum->ram += add->ram;
    sum->slab += add->slab;
}

typedef struct file_footprint_t {
    struct file_footprint_t *next;
    char *file;
    int mount;
    int section;
    mem_t mem;
} file_footprint;

static void file_footprint_list_add(file_footprint **list, char *file,
    int mount, int section, mem_t mem)
{
    file_footprint **f, *newf;
    for (f = list; *f; f = &(*f)->next)
    {
        if (!strcmp((*f)->file, file))
	{
	    /* a file contributes more than one section to the linked object,
	     * so just update its conribution size
	     */
	    (*f)->mem.flash_uncomp += mem.flash_uncomp;
	    (*f)->mem.flash += mem.flash;
	    (*f)->mem.ram += mem.ram;
	    (*f)->mem.slab += mem.slab;
	    return;
	}
	if (strcmp((*f)->file, file)>0)
	    break;
    }
    newf = zalloc(sizeof(**f));
    newf->file = strdup(file);
    newf->mount = mount;
    newf->section = section;
    newf->mem.flash_uncomp = mem.flash_uncomp;
    newf->mem.flash = mem.flash;
    newf->mem.ram = mem.ram;
    newf->mem.slab = mem.slab;
    newf->next = *f;
    *f = newf;
}

static void file_footprint_list_free(file_footprint *list)
{
    file_footprint *f = list, *tmp; 
    while (f)
    {
        tmp = f;
	f = f->next;
	free(tmp->file);
	free(tmp);
    }
}

static int file_is_elf(char *file)
{
    int is_elf = 0;
    char *res;
    res = sys_get(NULL, "file %s", file);
    if (strstr(res, " ELF "))
	is_elf = 1;
    return is_elf;
}

static int calc_slab_size(int ram)
{
    int slab;
    for (slab = 64; ram>slab; slab *= 2);
    return slab;
}

static int calc_size_elf(char *elf, int *slab)
{
    char *res = NULL;
    int text, data, bss, dec, hex;
    char filename[MAX_LINE];
    int ram;
    int ret;
    res = sys_get(NULL, "size %s 2>/dev/null", elf);
    ret = sscanf(res, " text data bss dec hex filename\n"
	"%d %d %d %d %x %s\n",
	&text, &data, &bss, &dec, &hex, filename); 
    if (ret!=6)
	rg_error(LEXIT, "Size returned invalid result '%s'", res);
    ram = dec;
    if (slab)
	*slab = calc_slab_size(ram);
    return dec;
}

static long long file_size(char *fname)
{
    struct stat st;
    int rc = stat(fname, &st);

    return rc ? -1 : st.st_size;
}

static int calc_size_gz(char *file)
{
    sys_exec("%s %s >.tmp.gz", ARCHIVER, file);
    return file_size(".tmp.gz");
}

static void rm_tmp_files(void)
{
    sys_exec("rm -f .tmp.*");
}

static void sum_section(file_footprint *main_file_list, int mounts,
    int sections, mem_t *mem)
{
    file_footprint *f;
    MZERO(*mem);
    for (f = main_file_list; f; f=f->next)
    {
	if (!(f->mount & mounts) || !(f->section & sections))
	    continue;
	add_mem(mem, &f->mem);
    }
}

static void add_ramdisk_free_space(file_footprint *main_file_list, mount_t *mount)
{
    int flash, ram;
    mem_t mem;
    sum_section(main_file_list, mount->mount, SECTION_ALL, &mem);
    ram = file_size(mount->img);
    flash = calc_size_gz(mount->img);
    if ((ram -= mem.ram) < 0)
	return;
    if ((flash -= mem.flash) < 0)
	flash = 0;
    mem.flash_uncomp = ram;
    mem.flash = flash;
    mem.ram = ram;
    mem.slab = ram;

    file_footprint_list_add(&main_file_list, "ramdisk free space",
	mount->mount, SECTION_OTHER, mem);
}

static void print_sep(int src_column)
{
    fprintf(report_fp, "-------\t-------\t-------\t-------\t-------%s\n",
	 src_column ? "\t-------" : "");
}

static char *get_src_file(char *destination)
{
    char **line;
    char **tokens = NULL;
    static char src_file[MAX_PATH];

    src_file[0] = '\0';

    if (!disk_img_lines)
	disk_img_lines = lines_read_fp(disk_img_fp);

    for (line = disk_img_lines; line && *line; line++)
    {
	/* Split the line into tokens */
	lines_str_split_ws(&tokens, *line);

	if (!strcmp(destination, tokens[2]))
	{
	    snprintf(src_file, sizeof(src_file), "%s", tokens[0]);
	    goto Exit;
	}
    }

Exit:
    lines_free(&tokens);
    return src_file;
}

static void print_hdr(char *mount, int src_column)
{
    fprintf(report_fp, "%s\n", mount);
    fprintf(report_fp, "=================\n");
    fprintf(report_fp, "flsh_un\tflash\tram\tslab\tname%s\n",
	src_column ? "\tsrc" : "");
    print_sep(src_column);
}

static void print_size(FILE* f, int sz)
{
    if (sz<1024)
	fprintf(f, "%6d ", sz);
    else
	fprintf(f, "%6dk", sz/1024);
}

static char *skip_slashes(char *str, int k)
{
    int i;
    char *c;

    for (i = k, c = str; *c && i; c++)
    {
	if (*c == '/')
	    i--;
    }

    if (!i)
	return c;
    return NULL;
}

static void accumulate_info(mem_t *mem, char *src_file)
{
    char *c;

    if ((c = skip_slashes(src_file, 3)))
	*c = '\0';

    lines_add_printf(&summary_lines, "%s %d %d %d %d", src_file,
	mem->flash_uncomp, mem->flash, mem->ram, mem->slab);
}

static void print_line(char *file, mem_t *mem, char *src_file)
{
    print_size(report_fp, mem->flash_uncomp);
    fprintf(report_fp, "\t");
    print_size(report_fp, mem->flash);
    fprintf(report_fp, "\t");
    print_size(report_fp, mem->ram);
    fprintf(report_fp, "\t");
    print_size(report_fp, mem->slab);
    fprintf(report_fp, "\t%s", file);
    if (src_file && *src_file)
	fprintf(report_fp, "\t%s", src_file);
    fprintf(report_fp, "\n");
}

static void print_sorted_lines(void)
{
    char **line;
    mem_t mem;
    char src_file[MAX_PATH], file[MAX_PATH];

    lines_sort(&sorted_lines);

    for (line = sorted_lines; line && *line; line++)
    {
	sscanf(*line, "%s %d %d %d %d %s", src_file, &mem.flash_uncomp,
	    &mem.flash, &mem.ram, &mem.slab, file);
	print_line(file, &mem, src_file);
    }
    lines_free(&sorted_lines);
}

static int compare(char **line1, char **line2)
{
    int s1, s2;

    sscanf(*line1, "%d", &s1);
    sscanf(*line2, "%d", &s2);

    return (s1 < s2 ? 1 : (s1 == s2 ? 0 : -1));
}


static void usage(void)
{
    printf("usage: footprint [options]\n"
        "footprint report generation.\n"
	"  should normally be run from inside pkg/build, or via\n"
	"  'make -C pkg/build do_footprint'.\n"
	"options:\n"
	"  --strip path_to_strip\n"
	"  --objcopy path_to_objcopy\n"
	"  --output footprint_report.txt\n"
	"  --output-list footprint_list_file.txt\n"
	"  --disk_image pkg/build/disk_image\n"
	"  --cramfs_dir cramfs_dir\n"
	"  --modfs_dir modfs_dir\n"
	"  --ramdisk_dir ramdisk_dir\n"
	"  --cramfs_img cramfs.img\n"
	"  --modfs_img mod.img\n"
	"  --linux_img ../../os/linux/vmlinux\n"
	"  --archiver archiver commandline (default is gzip -f9 -c)\n"
	"  --cramfs_in_flash (default set if CONFIG_RG_CRAMFS_IN_FLASH enabled)\n"
	);
    exit(1);
}

/* This function gets a file name and calculates its footprint. */
void compute_file_footprints(file_footprint **main_file_list, char *f,
    mount_t *mount, int *section, struct mem_t *mem)
{
    int is_elf = 0, is_module = 0, is_lib = 0;
    int flash = 0, flash_uncomp = 0, ram = 0, slab = 0;
    char *fbase;

    if (mount->mount == MOUNT_LINUX)
    {
	mem_t mem_;

	ram = calc_size_elf(mount->img, NULL);
	sys_exec("cp %s .tmp.linux", mount->img);
	sys_exec("%s -R .comment -R .note .tmp.linux", STRIP);
	sys_exec("%s -S -O binary .tmp.linux .tmp.linux.bin", OBJCOPY);
	sys_exec("bzip2 -c .tmp.linux.bin >.tmp.linux");
	if ((flash = file_size(".tmp.linux"))<0)
	    rg_error(LEXIT, "Not .tmp.linux file");
	/* all the file systems are embedded into the linux kernel image */
	if (*main_file_list)
	    sum_section(*main_file_list, ~MOUNT_LINUX, SECTION_ALL, &mem_);
	if ((flash -= mem_.flash)<0)
	    flash = 0;
	if ((ram -= mem_.flash)<0)
	    ram = 0;
	*section = SECTION_OTHER;
	flash_uncomp = ram;
	slab = ram;

	goto exit;
    }

    fbase = file_basename(f);

    if ((flash_uncomp = file_size(f))<0)
	rg_error(LEXIT, "Failed file_size(%s)", f);

    flash = calc_size_gz(f);
    is_elf = 1;

    if (!str_re(fbase, "\\.o$"))
    {
	*section = SECTION_MODULE;
	is_elf = 1;
	is_module = 1;
	/* modules are always loaded into ram */
	ram = flash_uncomp;
	slab = calc_slab_size(ram);
    }
    else if (!str_re(fbase, "(\\.so|\\.so\\..*)$"))
    {
	*section = SECTION_LIB;
	is_elf = 1;
	is_lib = 1;
	/* shared objects take up only cramfs space in ram */
	ram = flash;
	slab = flash;
    }
    else if (!str_re(fbase, "(\\.gif|\\.jpg|\\.|\\.jpeg|\\.png)$"))
    {
	*section = SECTION_IMAGE;
	/* other files take up only cramfs space in ram */
	ram = flash;
	slab = flash;
    }
    else if (file_is_elf(f))
    {
	*section = SECTION_EXEC;
	is_elf = 1;
	/* user mode executables take only the cramfs in ram,
	 * the rest is cached pages.
	 */
	ram = flash;
	slab = flash;
    }
    else
    {
	*section = SECTION_FILE;
	/* other files take up only cramfs space in ram */
	ram = flash;
	slab = flash;
    }

    if (is_cramfs_in_flash && !strcmp(mount->name, "cramfs"))
    {
	ram = 0;
	slab = 0;
    }
exit:
    mem->flash_uncomp = flash_uncomp;
    mem->flash = flash;
    mem->ram = ram;
    mem->slab = slab;

}

char *get_link_file(char *file)
{
    char *link_path = (char *)malloc(MAX_PATH);

    if(!file_is_symbolic_link(file))
	return NULL;

    int num = readlink(file, link_path, MAX_PATH);
    if (num < 1)
	return NULL;
    link_path[num]='\0';
    return link_path;
}

void get_object_section_list(char ***sections, char* object_name)
{
    char *section = NULL;
    char **lines = NULL, **line = NULL;

    lines = sys_get_lines(NULL, "readelf -S %s", object_name);

    for (line = lines; *line; line++)
    {
	if (!str_re_strs(*line, "^[ ]*\\[[ 0-9]*\\] (\\.[^ ]*) ",NULL,
	    &section, NULL) &&
	    (strcmp(object_name, LINUX_DIR "vmlinux") ||
	    !strstr(section, "debug")))
	{
	    lines_add_printf(sections, "%s", section);
	}
    }
    lines_free(&lines);
}

/* go over the link_map_path file and add the relevant objects to
 * all_footprint_files_list.
 */
void add_link_map_files(file_footprint **all_file_list, char *link_map_path,
    char *object_name)
{
    FILE *link_map_fp;
    char **lines = NULL, **line, **sections = NULL;
    mem_t mem;
    int is_section_countable = 0;

    get_object_section_list(&sections, object_name);

    if (!(link_map_fp = fopen(link_map_path, "r")))
	rg_error(LEXIT, "Failed opening link.map for %s", link_map_path);

    lines = lines_read_fp(link_map_fp);
    line = lines;

   /* reading only the sections that the stripped object contains. */
    while (line && *line)
    {
	char *size_str = NULL, *file = NULL, *section;

	/* check if a relevant section begins in this line */

	if (!str_re_strs(*line, "^(\\.[^ ]*)", &section, NULL))
	    is_section_countable = !! lines_search(sections, section);
    
	if (!is_section_countable)
	{
	    line++;
	    continue;
	}

	/* If the line contains a string that looks like:
	 * "0x4 mt_generic_proxy.o", we add the object to the list.
	 */
	if (!str_re_strs(*line,"(0x[0-9a-f]*) ([^ ]*\\.[ao])(\\([^ ]*\\))*$",
	    NULL, &size_str, &file, NULL))
	{
	    char *file_name = NULL, *sym_link_path = NULL;
	    int size;

	    sscanf(size_str, "%x", &size);
	    if (!file_exist(file))
	    {
	        /* if the file doesn't exist, we concatenate the link.map dir
		 * name. For example, "mt_main.o" will become
		 * "/home/rotem/rg.4_6/build.MONTEJADE/pkg/main/mt_main.o"
		 */
	        file_name = strdup(file_dirname(link_map_path, 1));
		str_cat(&file_name, file);
	    }
	    else
		file_name = file;
	    sym_link_path = get_link_file(file_name);
	    if (!sym_link_path)
		sym_link_path = file_name;
	    MZERO(mem);
	    mem.flash_uncomp = size;
	    file_footprint_list_add(all_file_list, sym_link_path, 
		MOUNT_NONE, SECTION_NONE, mem);
	}
	line++;
    }
    lines_free(&sections);
    lines_free(&lines);
    if (link_map_fp)
	fclose(link_map_fp);
}

static int detect_and_add_files_to_lists(mount_t *mount,
    file_footprint **main_file_list, file_footprint **all_file_list)
{
    char **files, **f;
    int total_mount_size = 0;
    if (mount->dir)
    {
	files = sys_get_lines(NULL, "find %s -type %c", mount->dir,
	    mount->filetype);

	for (f = files; *f; f++)
	{
	    char *curr_dir;
	    int section;

	    mem_t filemem;
	    char *filename;
	    char *filesrcpath;
	    char *filelinkmap = NULL;

	    if(mount->filetype == TYPE_SYMLINK)
	        filename = get_link_file(*f);
	    else
		filename = *f;

	    /* compute the footprints of the file */
	    compute_file_footprints(main_file_list, filename, mount, &section,
		&filemem);
	    total_mount_size += filemem.flash_uncomp;

	    /* add it to the main footprint files list */
	    if (main_file_list)
	    {
		file_footprint_list_add(main_file_list, filename,
		    mount->mount, section, filemem);
	    }

	    /* look for xxx.link.map file and parse it */

	    if (mount->filetype != TYPE_SYMLINK)
	    {
		/* getting the current dir name and removing the last 2
		 * directories: "pkg/build"
		 */
		curr_dir = getcwd((char *)NULL, 0);
		curr_dir[strlen(curr_dir)-strlen("pkg/build")] = '\0';

		if (!strcmp(filename, LINUX_DIR "vmlinux"))
		    filesrcpath = LINUX_DIR_ABS "vmlinux";
		else
		{
		    filesrcpath = strdup(get_src_file(filename));
		    if (!strcmp(filesrcpath,""))
			continue;

		    /* removing "rg/" from the source file. */
		    str_sed(&filesrcpath, "s|[^/]*/||");
		}
                filename = NULL;
		str_printf(&filename, "%s%s", curr_dir, filesrcpath);
	    }

	    str_printf(&filelinkmap, "%s.link.map", filename);

	    if (file_exist(filelinkmap))
	    {
		add_link_map_files(all_file_list, filelinkmap, strdup(*f));
	    }
	    else
	    {
		/* no link map */

		if (mount->mount == MOUNT_LINUX)
		{
		    /* in case where due to bug in ld, a link.map cannot be
		     * generated for the linux kernel (B46037), explicitly
		     * calculate the static modules.
		     */
		    mount_t static_modules = {MOUNT_MOD_STAT,
			"linux_static_mod", LINUX_DIR "modules_static", NULL, 0,
			TYPE_SYMLINK};

		    /* no need to add static modules to main_file_list beacuse
		     * they are not counted when summing mounts and sections
		     */
		    int static_modules_total =
			detect_and_add_files_to_lists(&static_modules, NULL,
			all_file_list);

		    filemem.flash_uncomp -= static_modules_total;
		}

		file_footprint_list_add(all_file_list, filename,
		    MOUNT_NONE, SECTION_NONE, filemem);
	    }
	}
    }
    if (mount->mount == MOUNT_RAMDISK && file_exist(mount->img))
	add_ramdisk_free_space(*main_file_list, mount);

    return total_mount_size;
}

void sum_by_pkg(file_footprint *all_file_list, char ***summed_lines)
{
    file_footprint *currf;
    char **dirs_in_currf_path = NULL, **dirs_in_nextf_path = NULL;
    int i, j, sum = 0;

    for (currf = all_file_list; currf; currf=currf->next)
    {
	lines_str_split(&dirs_in_currf_path, currf->file, "/", 0);
	lines_str_split(&dirs_in_nextf_path, currf->next ? currf->next->file :
	    "", "/", 0);

	sum += currf->mem.flash_uncomp;

	/* compare curr file path to next file path */
	for (i=0;
	    dirs_in_currf_path[i] && dirs_in_nextf_path[i] &&
	      !strcmp(dirs_in_currf_path[i], dirs_in_nextf_path[i]);
	    i++);

	for (j=lines_count(dirs_in_currf_path)-1; j>=i; j--)
	{

	    if (j>0 && (!strcmp(dirs_in_currf_path[j-1],"pkg") ||
		!strcmp(dirs_in_currf_path[j-1],"os") ||
		!strcmp(dirs_in_currf_path[j-1],"vendor")))
	    {
		lines_add_printf(summed_lines, "%10d rg/%s/%s", sum,
		    dirs_in_currf_path[j-1], dirs_in_currf_path[j]);
		sum = 0;
	    }
	    else if (j>2 && !strcmp(dirs_in_currf_path[j-3],"usr") &&
		!strcmp(dirs_in_currf_path[j-2],"local") &&
		!strcmp(dirs_in_currf_path[j-1],"openrg"))
	    {
		lines_add_printf(summed_lines, "%10d /%s/%s", sum,
		    "usr/local/openrg", dirs_in_currf_path[j]);
		sum = 0;
	    }
	}
	lines_free(&dirs_in_currf_path);
	lines_free(&dirs_in_nextf_path);
    }
}

static void sum_by_pkg_and_print(file_footprint *all_file_list)
{
    char **summed_lines = NULL;

    fprintf(report_fp, "\nSummary of uncompressed data, by pkg\n");
    fprintf(report_fp,   "====================================\n");

    sum_by_pkg(all_file_list, &summed_lines);
    lines_sort_func(&summed_lines, compare);
    lines_print_fp(summed_lines, report_fp);
    lines_free(&summed_lines);
}

int is_mount_valid(mount_t *mount)
{
    /* validate mount dir */
    mount->dir = strdup(mount->dir);
    if (!file_exist(mount->dir))
    {
	str_printf(&mount->dir, "%s/%s", disk_image, mount->dir);
	if (!file_exist(mount->dir))
	{
	    rg_error(LWARN,
		"Missing directory (%s) for mount (%s): skipped\n",
		mount->dir, mount->name);
	    return 0;
	}
    }

    /* validate mount img */
    mount->img = strdup(mount->img);
    if (!file_exist(mount->img))
    {
	str_printf(&mount->img, "%s/%s", disk_image, mount->img);
	if (!file_exist(mount->img))
	{
	    rg_error(LWARN,
		"Cannot find disk image (%s) for mount (%s): skipped\n",
		mount->img, mount->name);
	    return 0;
	}
    }
    return 1;
}

void print_newline(void)
{
    fprintf(report_fp, "\n");
}

void sum_by_mount_and_print(file_footprint *main_file_list)
{
    char *label = NULL;
    mem_t grand_grand_total;
    MZERO(grand_grand_total);
    mount_t *mount;
    section_t *section;

    print_hdr("Grand grand total", 0);
    for (mount = mount_list; mount->mount; mount++)
    {
	for (section = section_list; section->section; section++)
	{
	    mem_t mem;

	    if (!(mount->sections & section->section))
		continue;

	    sum_section(main_file_list, mount->mount, section->section, &mem);
	    add_mem(&grand_grand_total, &mem);
	    str_printf(&label, "%s: %s", mount->name, section->name);
	    print_line(label, &mem, 0);
	}
    }
    print_sep(0);
    print_line("grand grand total", &grand_grand_total, NULL);
    print_newline();
}

void sum_by_section_and_print(file_footprint *main_file_list, mount_t *mount)
{
    char *src_file, *label = NULL;
    mem_t grand_total;
    section_t *section;
    file_footprint *f;

    MZERO(grand_total);
    print_hdr(mount->name, 1);
    for (section = section_list; section->section; section++)
    {
	mem_t section_total;
	MZERO(section_total);

	if (!(mount->sections & section->section))
	    continue;
	for (f = main_file_list; f; f=f->next)
	{
	    if (!(f->mount & mount->mount) ||
		!(f->section & section->section))
	    {
		continue;
	    }
	    src_file = get_src_file(f->file);
	    if (src_file && *src_file)
	    {
		lines_add_printf(&sorted_lines, "%s %d %d %d %d %s",
		    src_file, (f->mem).flash_uncomp, (f->mem).flash,
		    (f->mem).ram, (f->mem).slab, f->file);
	    }
	    else
		print_line(f->file, &f->mem, src_file);
	    if (src_file && *src_file)
		accumulate_info(&f->mem, src_file);
	}
	print_sorted_lines();
	print_sep(1);
	str_printf(&label, "total: %s %s", mount->name, section->name);
	sum_section(main_file_list, mount->mount, section->section,
	    &section_total);
	print_line(label, &section_total, NULL);
	print_sep(1);
    }
    str_printf(&label, "grand total: %s", mount->name);
    sum_section(main_file_list, mount->mount, SECTION_ALL, &grand_total);
    print_line(label, &grand_total, NULL);
    fprintf(report_fp, "\n");
}


void print_footprint_file_list(file_footprint *all_file_list, char *file_name)
{
    file_footprint *f;
    FILE *list_file_fp;

    if (!(list_file_fp = fopen(file_name, "w")))
	rg_error(LEXIT, "Failed opening %s\n", file_name);

    fprintf(list_file_fp,"     size                    file\n");
    fprintf(list_file_fp,"     ====                    ====\n");

    for (f = all_file_list; f; f=f->next)
    {
	fprintf(list_file_fp, "%10d   ", f->mem.flash_uncomp);
	fprintf(list_file_fp, "%s\n", f->file);
    }

    fclose(list_file_fp);
}

int main(int argc, char *argv[])
{
    char *report_file = NULL;
    char *list_file_name = NULL;
    mount_t *mount;
    char *s = NULL, *s2 = NULL;
    file_footprint *main_file_list = NULL;
    file_footprint *all_file_list = NULL;

    sys_set_expect(SYS_EXPECT_ZERO);
    rm_tmp_files();
    argv++;
    for (; *argv && *argv[0]=='-'; argv++)
    {
	if (!strcmp(*argv, "--archiver"))
	{
	    argv++;
	    ARCHIVER = *argv;
	}
	else if (!strcmp(*argv, "--strip"))
	{
	    argv++;
	    STRIP = *argv;
	}
	else if (!strcmp(*argv, "--objcopy"))
	{
	    argv++;
	    OBJCOPY = *argv;
	}
	else if (!strcmp(*argv, "--output"))
	{
	    argv++;
	    report_file = *argv;
	}
	else if (!strcmp(*argv, "--output-list"))
	{
	    argv++;
	    list_file_name = *argv;
	}
	else if (!strcmp(*argv, "--disk_image"))
	{
	    argv++;
	    disk_image = *argv;
	}
	else if (!strcmp(*argv, "--cramfs_in_flash"))
	{
	    is_cramfs_in_flash = 1;
	}
	else if (!str_re_strs(*argv,
	    "^--(cramfs|modfs|ramdisk|linux)_(dir|img)$", NULL, &s, &s2))
	{
	    mount = get_mount(s);
	    argv++;
	    if (!strcmp(s2, "dir"))
		mount->dir = *argv;
	    else
		mount->img = *argv;
	}
	else
	    usage();
    }
    if (*argv)
	usage();

    if (!disk_image)
	disk_image = strdup("disk_image");

    if (report_file)
    {
	if (!(report_fp = fopen(report_file, "w")))
	    rg_error(LEXIT, "Failed opening report file");
    }
    else
	report_fp = stdout;

    if (!(disk_img_fp = fopen("disk_image_file.txt", "r")))
	rg_error(LEXIT, "failed opening disk_image_file.txt");

    fprintf(report_fp, "CRAMFS_IN_FLASH: %s\n\n", is_cramfs_in_flash? "y" : "n");
    /* arguments are valid */

    for (mount = mount_list; mount->mount; mount++)
    {
	if (is_mount_valid(mount))
	{
	    detect_and_add_files_to_lists(mount, &main_file_list, &all_file_list);
	    sum_by_section_and_print(main_file_list, mount);
	}
    }

    sum_by_mount_and_print(main_file_list);

    /* second part: display footprint for each directory */

    print_footprint_file_list(all_file_list, list_file_name);
    sum_by_pkg_and_print(all_file_list);

    rm_tmp_files();
    fclose(report_fp);
    fclose(disk_img_fp);
    file_footprint_list_free(all_file_list);
    file_footprint_list_free(main_file_list);
    return 0;
}


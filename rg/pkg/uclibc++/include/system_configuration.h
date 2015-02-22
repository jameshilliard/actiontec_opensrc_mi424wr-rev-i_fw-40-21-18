#include <rg_config.h>

/*
 * Version Number
 */
#define __UCLIBCXX_MAJOR__ 0
#define __UCLIBCXX_MINOR__ 1
#define __UCLIBCXX_SUBLEVEL__ 11
#ifdef CONFIG_ARM
  #ifndef __TARGET_arm__
    #define __TARGET_arm__ 1
  #endif
#else
  #undef __TARGET_arm__
#endif
#ifdef CONFIG_M386
  #define __TARGET_i386__ 1
#else
  #undef __TARGET_i386__
#endif
#ifdef CONFIG_MIPS
  #define __TARGET_mips__ 1
#else
  #undef __TARGET_mips__
#endif
#undef __TARGET_powerpc__

/*
 * Target Architecture Features and Options
 */
#define __HAVE_ELF__ 1
#ifdef CONFIG_ARM
  #define __TARGET_ARCH__ "arm"
  #define __CONFIG_GENERIC_ARM__ 1
  #undef __CONFIG_ARM610__
  #undef __CONFIG_ARM710__
  #undef __CONFIG_ARM720T__
  #undef __CONFIG_ARM920T__
  #undef __CONFIG_ARM922T__
  #undef __CONFIG_ARM926T__
  #undef __CONFIG_ARM_SA110__
  #undef __CONFIG_ARM_SA1100__
  #undef __CONFIG_ARM_XSCALE__
  #undef __CONFIG_GENERIC_386__
#endif
#if CONFIG_M386
  #define __TARGET_ARCH__ "i386"
  #define __CONFIG_GENERIC_386__ 1
#else
  #undef __CONFIG_GENERIC_386__
#endif
#ifdef CONFIG_MIPS
  #define __TARGET_ARCH__ "mips"
  #define __ARCH_CFLAGS__ "-mno-split-addresses"
  #define __CONFIG_MIPS_ISA_1__ 1
  #undef __CONFIG_MIPS_ISA_2__
  #undef __CONFIG_MIPS_ISA_3__
  #undef __CONFIG_MIPS_ISA_4__
  #undef __CONFIG_MIPS_ISA_MIPS32__
  #undef __CONFIG_MIPS_ISA_MIPS64__
#endif

#undef __CONFIG_386__
#undef __CONFIG_486__
#undef __CONFIG_586__
#undef __CONFIG_586MMX__
#undef __CONFIG_686__
#undef __CONFIG_PENTIUMIII__
#undef __CONFIG_PENTIUM4__
#undef __CONFIG_K6__
#undef __CONFIG_K7__
#undef __CONFIG_CRUSOE__
#undef __CONFIG_WINCHIPC6__
#undef __CONFIG_WINCHIP2__
#undef __CONFIG_CYRIXIII__
#define __ARCH_LITTLE_ENDIAN__ 1
#undef __ARCH_BIG_ENDIAN__
#define __UCLIBCXX_HAS_FLOATS__ 1
#define __WARNINGS__ "-Wall"
#define __HAVE_DOT_CONFIG__ 1

/*
 * String and I/O Stream Support
 */
#undef __UCLIBCXX_HAS_WCHAR__
#define __UCLIBCXX_IOSTREAM_BUFSIZE__ 32
#define __UCLIBCXX_HAS_LFS__ 1
#define __UCLIBCXX_SUPPORT_CDIR__ 1
#define __UCLIBCXX_SUPPORT_CIN__ 1
#define __UCLIBCXX_SUPPORT_COUT__ 1
#define __UCLIBCXX_SUPPORT_CERR__ 1
#undef __UCLIBCXX_SUPPORT_CLOG__

/*
 * STL and Code Expansion
 */
#define __UCLIBCXX_STL_BUFFER_SIZE__ 32
#define __UCLIBCXX_CODE_EXPANSION__ 1
#undef __UCLIBCXX_EXPAND_CONSTRUCTORS_DESTRUCTORS__
#define __UCLIBCXX_EXPAND_STRING_CHAR__ 1
#define __UCLIBCXX_EXPAND_VECTOR_BASIC__ 1
#define __UCLIBCXX_EXPAND_IOS_CHAR__ 1
#define __UCLIBCXX_EXPAND_STREAMBUF_CHAR__ 1
#define __UCLIBCXX_EXPAND_ISTREAM_CHAR__ 1
#define __UCLIBCXX_EXPAND_OSTREAM_CHAR__ 1
#define __UCLIBCXX_EXPAND_FSTREAM_CHAR__ 1
#define __UCLIBCXX_EXPAND_SSTREAM_CHAR__ 1

/*
 * Library Installation Options
 */
#define __UCLIBCXX_RUNTIME_PREFIX__ "/usr/$(TARGET_ARCH)-linux-uclibc"
#define __UCLIBCXX_RUNTIME_INCLUDE_SUBDIR__ "/include"
#define __UCLIBCXX_RUNTIME_LIB_SUBDIR__ "/lib"
#define __UCLIBCXX_RUNTIME_BIN_SUBDIR__ "/bin"
#define __UCLIBCXX_EXCEPTION_SUPPORT__ 1
#define __IMPORT_LIBSUP__ 1
#define __IMPORT_LIBGCC_EH__ 1
#define __BUILD_STATIC_LIB__ 1
#undef __BUILD_ONLY_STATIC_LIB__
#undef __DODEBUG__

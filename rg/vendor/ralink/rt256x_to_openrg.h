#ifndef _RT256X_TO_OPENRG_H_
#define _RT256X_TO_OPENRG_H_

/* Base of rt_config.h and rtmp_def.h */

#undef BOOLEAN
#define BOOLEAN u8

#undef UCHAR
#define UCHAR u8

#undef ULONG
#define ULONG u32

#undef UINT
#define UINT u32

#undef LONG
#define LONG int

#undef ULONGLONG
#define ULONGLONG u64

#undef USHORT
#define USHORT u16

#undef ETH_LENGTH_OF_ADDRESS
#define ETH_LENGTH_OF_ADDRESS 6

/* Group rekey interval (copied from wpa.h) */
#ifndef TIME_REKEY
#define TIME_REKEY              0
#define PKT_REKEY               1
#define DISABLE_REKEY           2
#define MAX_REKEY               2
#endif

#endif


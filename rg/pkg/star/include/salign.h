/*
 * This file has been generated automatically
 * by @(#)align_test.c  1.15 02/05/20 Copyright 1995 J. Schilling
 * do not edit by hand.
 */
#ifndef _UTYPES_H
#include <utypes.h>
#endif

#if defined(CONFIG_IXP425) || defined(CONFIG_SL2312_ASIC) || \
    defined(CONFIG_CX8620X_COMMON) || defined(CONFIG_BCM963XX_COMMON) || \
    defined(CONFIG_COMCERTO) || defined(CONFIG_ARCH_SOLOS) || \
    defined(CONFIG_RG_UML)

#define ALIGN_SHORT     2       /* alignment value for (short *)        */
#define ALIGN_SMASK     1       /* alignment mask  for (short *)        */
#define SIZE_SHORT      2       /* sizeof (short)                       */

#define ALIGN_INT       4       /* alignment value for (int *)          */
#define ALIGN_IMASK     3       /* alignment mask  for (int *)          */
#define SIZE_INT        4       /* sizeof (int)                         */

#define ALIGN_LONG      4       /* alignment value for (long *)         */
#define ALIGN_LMASK     3       /* alignment mask  for (long *)         */
#define SIZE_LONG       4       /* sizeof (long)                        */

#define ALIGN_LLONG     4       /* alignment value for (long long *)    */
#define ALIGN_LLMASK    3       /* alignment mask  for (long long *)    */
#define SIZE_LLONG      8       /* sizeof (long long)                   */

#define ALIGN_FLOAT     4       /* alignment value for (float *)        */
#define ALIGN_FMASK     3       /* alignment mask  for (float *)        */
#define SIZE_FLOAT      4       /* sizeof (float)                       */

#define ALIGN_DOUBLE    4       /* alignment value for (double *)       */
#define ALIGN_DMASK     3       /* alignment mask  for (double *)       */
#define SIZE_DOUBLE     8       /* sizeof (double)                      */

#define ALIGN_PTR       4       /* alignment value for (pointer *)      */
#define ALIGN_PMASK     3       /* alignment mask  for (pointer *)      */
#define SIZE_PTR        4       /* sizeof (pointer)                     */

#endif

#if defined(CONFIG_MPC8272ADS) || defined(CONFIG_MPC8349_ITX) \
    || defined(CONFIG_SIBYTE_SB1250) || defined(CONFIG_FUSIV_VX160) \
    || defined(CONFIG_DANUBE)

#define ALIGN_SHORT     2       /* alignment value for (short *)        */
#define ALIGN_SMASK     1       /* alignment mask  for (short *)        */
#define SIZE_SHORT      2       /* sizeof (short)                       */

#define ALIGN_INT       4       /* alignment value for (int *)          */
#define ALIGN_IMASK     3       /* alignment mask  for (int *)          */
#define SIZE_INT        4       /* sizeof (int)                         */

#define ALIGN_LONG      4       /* alignment value for (long *)         */
#define ALIGN_LMASK     3       /* alignment mask  for (long *)         */
#define SIZE_LONG       4       /* sizeof (long)                        */

#define ALIGN_LLONG     8       /* alignment value for (long long *)    */
#define ALIGN_LLMASK    7       /* alignment mask  for (long long *)    */
#define SIZE_LLONG      8       /* sizeof (long long)                   */

#define ALIGN_FLOAT     4       /* alignment value for (float *)        */
#define ALIGN_FMASK     3       /* alignment mask  for (float *)        */
#define SIZE_FLOAT      4       /* sizeof (float)                       */

#define ALIGN_DOUBLE    8       /* alignment value for (double *)       */
#define ALIGN_DMASK     7       /* alignment mask  for (double *)       */
#define SIZE_DOUBLE     8       /* sizeof (double)                      */

#define ALIGN_PTR       4       /* alignment value for (pointer *)      */
#define ALIGN_PMASK     3       /* alignment mask  for (pointer *)      */
#define SIZE_PTR        4       /* sizeof (pointer)                     */

#endif

#if defined(CONFIG_CAVIUM_OCTEON)

#define ALIGN_SHORT     2       /* alignment value for (short *)        */
#define ALIGN_SMASK     1       /* alignment mask  for (short *)        */
#define SIZE_SHORT      2       /* sizeof (short)                       */

#define ALIGN_INT       4       /* alignment value for (int *)          */
#define ALIGN_IMASK     3       /* alignment mask  for (int *)          */
#define SIZE_INT        4       /* sizeof (int)                         */

#define ALIGN_LONG      8       /* alignment value for (long *)         */
#define ALIGN_LMASK     7       /* alignment mask  for (long *)         */
#define SIZE_LONG       8       /* sizeof (long)                        */

#define ALIGN_LLONG     8       /* alignment value for (long long *)    */
#define ALIGN_LLMASK    7       /* alignment mask  for (long long *)    */
#define SIZE_LLONG      8       /* sizeof (long long)                   */

#define ALIGN_FLOAT     4       /* alignment value for (float *)        */
#define ALIGN_FMASK     3       /* alignment mask  for (float *)        */
#define SIZE_FLOAT      4       /* sizeof (float)                       */

#define ALIGN_DOUBLE    8       /* alignment value for (double *)       */
#define ALIGN_DMASK     7       /* alignment mask  for (double *)       */
#define SIZE_DOUBLE     8       /* sizeof (double)                      */

#define ALIGN_PTR       8       /* alignment value for (pointer *)      */
#define ALIGN_PMASK     7       /* alignment mask  for (pointer *)      */
#define SIZE_PTR        8       /* sizeof (pointer)                     */

#endif

#ifndef ALIGN_SHORT
#error architecture is not supported. run align_test to get align information
#endif

/*
 * There used to be a cast to an int but we get a warning from GCC.
 * This warning message from GCC is wrong.
 * Believe me that this macro would even be usable if I would cast to short.
 * In order to avoid this warning, we are now using UIntptr_t
 */
#define xaligned(a, s)          ((((UIntptr_t)(a)) & s) == 0 )
#define x2aligned(a, b, s)      (((((UIntptr_t)(a)) | ((UIntptr_t)(b))) & s) == 0 )

#define saligned(a)             xaligned(a, ALIGN_SMASK)
#define s2aligned(a, b)         x2aligned(a, b, ALIGN_SMASK)

#define ialigned(a)             xaligned(a, ALIGN_IMASK)
#define i2aligned(a, b)         x2aligned(a, b, ALIGN_IMASK)

#define laligned(a)             xaligned(a, ALIGN_LMASK)
#define l2aligned(a, b)         x2aligned(a, b, ALIGN_LMASK)

#define llaligned(a)            xaligned(a, ALIGN_LLMASK)
#define ll2aligned(a, b)        x2aligned(a, b, ALIGN_LLMASK)

#define faligned(a)             xaligned(a, ALIGN_FMASK)
#define f2aligned(a, b)         x2aligned(a, b, ALIGN_FMASK)

#define daligned(a)             xaligned(a, ALIGN_DMASK)
#define d2aligned(a, b)         x2aligned(a, b, ALIGN_DMASK)

#define paligned(a)             xaligned(a, ALIGN_PMASK)
#define p2aligned(a, b)         x2aligned(a, b, ALIGN_PMASK)


/*
 * There used to be a cast to an int but we get a warning from GCC.
 * This warning message from GCC is wrong.
 * Believe me that this macro would even be usable if I would cast to short.
 * In order to avoid this warning, we are now using UIntptr_t
 */
#define xalign(x, a, m)         ( ((char *)(x)) + ( (a) - 1 - ((((UIntptr_t)(x))-1)&(m)))

#define salign(x)               xalign((x), ALIGN_SHORT, ALIGN_SMASK)
#define ialign(x)               xalign((x), ALIGN_INT, ALIGN_IMASK)
#define lalign(x)               xalign((x), ALIGN_LONG, ALIGN_LMASK)
#define llalign(x)              xalign((x), ALIGN_LLONG, ALIGN_LLMASK)
#define falign(x)               xalign((x), ALIGN_FLOAT, ALIGN_FMASK)
#define dalign(x)               xalign((x), ALIGN_DOUBLE, ALIGN_DMASK)
#define palign(x)               xalign((x), ALIGN_PTR, ALIGN_PMASK)


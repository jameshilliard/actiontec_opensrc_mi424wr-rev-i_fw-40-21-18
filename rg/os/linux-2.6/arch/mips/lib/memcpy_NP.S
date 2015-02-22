/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Unified implementation of memcpy, memmove and the __copy_user backend.
 *
 * Copyright (C) 1998, 99, 2000, 01, 2002 Ralf Baechle (ralf@gnu.org)
 * Copyright (C) 1999, 2000, 01, 2002 Silicon Graphics, Inc.
 * Copyright (C) 2002 Broadcom, Inc.
 *   memcpy/copy_user author: Mark Vandevoorde
 *
 * Mnemonic names for arguments to memcpy/__copy_user
 */
#include <linux/config.h>
#include <asm/asm.h>
#include <asm/asm-offsets.h>
#include <asm/regdef.h>

#define dst a0
#define src a1
#define len a2

/*
 * Spec
 *
 * memcpy copies len bytes from src to dst and sets v0 to dst.
 * It assumes that
 *   - src and dst don't overlap
 *   - src is readable
 *   - dst is writable
 * memcpy uses the standard calling convention
 *
 * __copy_user copies up to len bytes from src to dst and sets a2 (len) to
 * the number of uncopied bytes due to an exception caused by a read or write.
 * __copy_user assumes that src and dst don't overlap, and that the call is
 * implementing one of the following:
 *   copy_to_user
 *     - src is readable  (no exceptions when reading src)
 *   copy_from_user
 *     - dst is writable  (no exceptions when writing dst)
 * __copy_user uses a non-standard calling convention; see
 * include/asm-mips/uaccess.h
 *
 * When an exception happens on a load, the handler must
 # ensure that all of the destination buffer is overwritten to prevent
 * leaking information to user mode programs.
 */

/*
 * Implementation
 */

/*
 * The exception handler for loads requires that:
 *  1- AT contain the address of the byte just past the end of the source
 *     of the copy,
 *  2- src_entry <= src < AT, and
 *  3- (dst - src) == (dst_entry - src_entry),
 * The _entry suffix denotes values when __copy_user was called.
 *
 * (1) is set up up by uaccess.h and maintained by not writing AT in copy_user
 * (2) is met by incrementing src by the number of bytes copied
 * (3) is met by not doing loads between a pair of increments of dst and src
 *
 * The exception handlers for stores adjust len (if necessary) and return.
 * These handlers do not need to overwrite any data.
 *
 * For __rmemcpy and memmove an exception is always a kernel bug, therefore
 * they're not protected.
 */

#define EXC(inst_reg,addr,handler)		\
9:	inst_reg, addr;				\
	.section __ex_table,"a";		\
	PTR	9b, handler;			\
	.previous

/*
 * Only on the 64-bit kernel we can made use of 64-bit registers.
 */
#ifdef CONFIG_MIPS64
#define USE_DOUBLE
#endif

#ifdef USE_DOUBLE

#define LOAD   ld
#define LOADL  ldl
#define LOADR  ldr
#define STOREL sdl
#define STORER sdr
#define STORE  sd
#define ADD    daddu
#define SUB    dsubu
#define SRL    dsrl
#define SRA    dsra
#define SLL    dsll
#define SLLV   dsllv
#define SRLV   dsrlv
#define NBYTES 8
#define LOG_NBYTES 3

/* 
 * As we are sharing code base with the mips32 tree (which use the o32 ABI
 * register definitions). We need to redefine the register definitions from
 * the n64 ABI register naming to the o32 ABI register naming.
 */
#undef t0
#undef t1
#undef t2
#undef t3
#define t0	$8
#define t1	$9
#define t2	$10
#define t3	$11
#define t4	$12
#define t5	$13
#define t6	$14
#define t7	$15
	
#else

#define LOAD   lw
#define LOADL  lwl
#define LOADR  lwr
#define STOREL swl
#define STORER swr
#define STORE  sw
#define ADD    addu
#define SUB    subu
#define SRL    srl
#define SLL    sll
#define SRA    sra
#define SLLV   sllv
#define SRLV   srlv
#define NBYTES 4
#define LOG_NBYTES 2

#endif /* USE_DOUBLE */

#ifdef CONFIG_CPU_LITTLE_ENDIAN
#define LDFIRST LOADR
#define LDREST  LOADL
#define STFIRST STORER
#define STREST  STOREL
#define SHIFT_DISCARD SLLV
#else
#define LDFIRST LOADL
#define LDREST  LOADR
#define STFIRST STOREL
#define STREST  STORER
#define SHIFT_DISCARD SRLV
#endif

#define FIRST(unit) ((unit)*NBYTES)
#define REST(unit)  (FIRST(unit)+NBYTES-1)
#define UNIT(unit)  FIRST(unit)

#define ADDRMASK (NBYTES-1)

	.text
	.set	noreorder
	.set	noat

/*
 * A combined memcpy/__copy_user
 * __copy_user sets len to 0 for success; else to an upper bound of
 * the number of uncopied bytes.
 * memcpy sets v0 to dst.
 */
	.align	5
LEAF(memcpy)					/* a0=dst a1=src a2=len */
	move	v0, dst				/* return value */
__memcpy:
FEXPORT(__copy_user)
	/*
	 * Note: dst & src may be unaligned, len may be 0
	 * Temps
	 */
#define rem t8

	/*
	 * The "issue break"s below are very approximate.
	 * Issue delays for dcache fills will perturb the schedule, as will
	 * load queue full replay traps, etc.
	 *
	 * If len < NBYTES use byte operations.
	 */
	PREF(	0, 0(src) )
	PREF(	1, 0(dst) )
	sltu	t2, len, NBYTES
	and	t1, dst, ADDRMASK
	PREF(	0, 1*32(src) )
	PREF(	1, 1*32(dst) )
	bnez	t2, copy_bytes_checklen
	 and	t0, src, ADDRMASK
	PREF(	0, 2*32(src) )
	PREF(	1, 2*32(dst) )
	bnez	t1, dst_unaligned
	 nop
	bnez	t0, src_unaligned_dst_aligned
	/*
	 * use delay slot for fall-through
	 * src and dst are aligned; need to compute rem
	 */
both_aligned:
	 SRL	t0, len, LOG_NBYTES+3    # +3 for 8 units/iter
	beqz	t0, cleanup_both_aligned # len < 8*NBYTES
	 and	rem, len, (8*NBYTES-1)	 # rem = len % (8*NBYTES)
	PREF(	0, 3*32(src) )
	PREF(	1, 3*32(dst) )
	.align	4
1:
EXC(	LOAD	t0, UNIT(0)(src),	l_exc)
EXC(	LOAD	t1, UNIT(1)(src),	l_exc_copy)
EXC(	LOAD	t2, UNIT(2)(src),	l_exc_copy)
EXC(	LOAD	t3, UNIT(3)(src),	l_exc_copy)
	SUB	len, len, 8*NBYTES
EXC(	LOAD	t4, UNIT(4)(src),	l_exc_copy)
EXC(	LOAD	t7, UNIT(5)(src),	l_exc_copy)
EXC(	STORE	t0, UNIT(0)(dst),	s_exc_p8u)
EXC(	STORE	t1, UNIT(1)(dst),	s_exc_p7u)
EXC(	LOAD	t0, UNIT(6)(src),	l_exc_copy)
EXC(	LOAD	t1, UNIT(7)(src),	l_exc_copy)
	ADD	src, src, 8*NBYTES
	ADD	dst, dst, 8*NBYTES
EXC(	STORE	t2, UNIT(-6)(dst),	s_exc_p6u)
EXC(	STORE	t3, UNIT(-5)(dst),	s_exc_p5u)
EXC(	STORE	t4, UNIT(-4)(dst),	s_exc_p4u)
EXC(	STORE	t7, UNIT(-3)(dst),	s_exc_p3u)
EXC(	STORE	t0, UNIT(-2)(dst),	s_exc_p2u)
EXC(	STORE	t1, UNIT(-1)(dst),	s_exc_p1u)
	PREF(	0, 8*32(src) )
	PREF(	1, 8*32(dst) )
	bne	len, rem, 1b
	 nop

	/*
	 * len == rem == the number of bytes left to copy < 8*NBYTES
	 */
cleanup_both_aligned:
	beqz	len, done
	 sltu	t0, len, 4*NBYTES
	bnez	t0, less_than_4units
	 and	rem, len, (NBYTES-1)	# rem = len % NBYTES
	/*
	 * len >= 4*NBYTES
	 */
EXC(	LOAD	t0, UNIT(0)(src),	l_exc)
EXC(	LOAD	t1, UNIT(1)(src),	l_exc_copy)
EXC(	LOAD	t2, UNIT(2)(src),	l_exc_copy)
EXC(	LOAD	t3, UNIT(3)(src),	l_exc_copy)
	SUB	len, len, 4*NBYTES
	ADD	src, src, 4*NBYTES
EXC(	STORE	t0, UNIT(0)(dst),	s_exc_p4u)
EXC(	STORE	t1, UNIT(1)(dst),	s_exc_p3u)
EXC(	STORE	t2, UNIT(2)(dst),	s_exc_p2u)
EXC(	STORE	t3, UNIT(3)(dst),	s_exc_p1u)
	beqz	len, done
	 ADD	dst, dst, 4*NBYTES
less_than_4units:
	/*
	 * rem = len % NBYTES
	 */
	beq	rem, len, copy_bytes
	 nop
1:
EXC(	LOAD	t0, 0(src),		l_exc)
	ADD	src, src, NBYTES
	SUB	len, len, NBYTES
EXC(	STORE	t0, 0(dst),		s_exc_p1u)
	bne	rem, len, 1b
	 ADD	dst, dst, NBYTES

	/*
	 * src and dst are aligned, need to copy rem bytes (rem < NBYTES)
	 * A loop would do only a byte at a time with possible branch
	 * mispredicts.  Can't do an explicit LOAD dst,mask,or,STORE
	 * because can't assume read-access to dst.  Instead, use
	 * STREST dst, which doesn't require read access to dst.
	 *
	 * This code should perform better than a simple loop on modern,
	 * wide-issue mips processors because the code has fewer branches and
	 * more instruction-level parallelism.
	 */
#define bits t2
	beqz	len, done
	nop
	j	copy_bytes
	nop
dst_unaligned:
	/*
	 * dst is unaligned
	 * t0 = src & ADDRMASK
	 * t1 = dst & ADDRMASK; T1 > 0
	 * len >= NBYTES
	 *
	 * Copy enough bytes to align dst
	 * Set match = (src and dst have same alignment)
	 */
#define match rem
#define COPY_BYTE1(src,dst)			\
EXC(	lbu	t0, 0(src), l_exc);	\
	SUB	len, len, 1;		\
EXC(	 sb	t0, 0(dst), s_exc_p1) \
	ADD src, src, 1;		\
	beqz	len, done;		\
	ADD dst, dst,1;		\

	ADD t2, zero,NBYTES
	SUB t3, t2, t1			
copy1:
	COPY_BYTE1(src,dst)
	SUB t3, t3 , 1
	bne t3, zero, copy1 
	nop

	xor match, t0, t1
	beqz	match, both_aligned
	nop
src_unaligned_dst_aligned:
	SRL	t0, len, LOG_NBYTES+2    # +2 for 4 units/iter
	PREF(	0, 3*32(src) )
	beqz	t0, cleanup_src_unaligned
	 and	rem, len, (4*NBYTES-1)   # rem = len % 4*NBYTES
	PREF(	1, 3*32(dst) )
1:
EXC( lbu 	t0, 0(src), l_exc)
EXC( lbu 	t4, 1(src), l_exc_copy)
EXC( lbu  t5, 2(src), l_exc_copy)
EXC( lbu  t6, 3(src), l_exc_copy)
	sll	t0 , 8
	or	t0, t4
	sll 	t0, 8
	or	t0, t5
	sll 	t0, 8
	or	t0, t6
	ADD src, src, 4
EXC( lbu 	t1, 0(src), l_exc_copy)
EXC( lbu 	t4, 1(src), l_exc_copy)
EXC( lbu  t5, 2(src), l_exc_copy)
EXC( lbu  t6, 3(src), l_exc_copy)
	sll	t1 , 8
	or	t1, t4
	sll 	t1, 8
	or	t1, t5
	sll 	t1, 8
	or	t1, t6
	ADD src, src, 4
EXC( lbu 	t2, 0(src), l_exc_copy)
EXC( lbu 	t4, 1(src), l_exc_copy)
EXC( lbu  t5, 2(src), l_exc_copy)
EXC( lbu  t6, 3(src), l_exc_copy)
	sll	t2 , 8
	or	t2, t4
	sll 	t2, 8
	or	t2, t5
	sll 	t2, 8
	or	t2, t6
	ADD src, src, 4
EXC( lbu 	t3, 0(src), l_exc_copy)
EXC( lbu 	t4, 1(src), l_exc_copy)
EXC( lbu  t5, 2(src), l_exc_copy)
EXC( lbu  t6, 3(src), l_exc_copy)
	sll	t3 , 8
	or	t3, t4
	sll 	t3, 8
	or	t3, t5
	sll 	t3, 8
	or	t3, t6
	ADD src, src, 4
	
	SUB     len, len, 4*NBYTES
EXC(	STORE	t0, UNIT(0)(dst),	s_exc_p4u)
EXC(	STORE	t1, UNIT(1)(dst),	s_exc_p3u)
EXC(	STORE	t2, UNIT(2)(dst),	s_exc_p2u)
EXC(	STORE	t3, UNIT(3)(dst),	s_exc_p1u)
	bne	len, rem, 1b
	 ADD	dst, dst, 4*NBYTES
	



cleanup_src_unaligned:
	beqz	len, done
	 and	rem, len, NBYTES-1  # rem = len % NBYTES
	beq	rem, len, copy_bytes
	 nop
1:
EXC( lbu 	t0, 0(src), l_exc)
EXC( lbu 	t4, 1(src), l_exc_copy)
EXC( lbu  t5, 2(src), l_exc_copy)
EXC( lbu  t6, 3(src), l_exc_copy)
	sll	t0 , 8
	or	t0, t4
	sll 	t0, 8
	or	t0, t5
	sll 	t0, 8
	or	t0, t6

	ADD	src, src, NBYTES
	SUB	len, len, NBYTES
EXC(	STORE	t0, 0(dst),		s_exc_p1u)
	bne	len, rem, 1b
	 ADD	dst, dst, NBYTES

copy_bytes_checklen:
	beqz	len, done
	 nop
copy_bytes:
	/* 0 < len < NBYTES  */
#define COPY_BYTE(N)			\
EXC(	lb	t0, N(src), l_exc);	\
	SUB	len, len, 1;		\
	beqz	len, done;		\
EXC(	 sb	t0, N(dst), s_exc_p1)

	COPY_BYTE(0)
	COPY_BYTE(1)
#ifdef USE_DOUBLE
	COPY_BYTE(2)
	COPY_BYTE(3)
	COPY_BYTE(4)
	COPY_BYTE(5)
#endif
EXC(	lb	t0, NBYTES-2(src), l_exc)
	SUB	len, len, 1
	jr	ra
EXC(	 sb	t0, NBYTES-2(dst), s_exc_p1)
done:
	jr	ra
	 nop
	END(memcpy)

l_exc_copy:
	/*
	 * Copy bytes from src until faulting load address (or until a
	 * lb faults)
	 *
	 * When reached by a faulting LDFIRST/LDREST, THREAD_BUADDR($28)
	 * may be more than a byte beyond the last address.
	 * Hence, the lb below may get an exception.
	 *
	 * Assumes src < THREAD_BUADDR($28)
	 */
	LOAD	t0, TI_TASK($28)
	 nop
	LOAD	t0, THREAD_BUADDR(t0)
1:
EXC(	lb	t1, 0(src),	l_exc)
	ADD	src, src, 1
	sb	t1, 0(dst)	# can't fault -- we're copy_from_user
	bne	src, t0, 1b
	 ADD	dst, dst, 1
l_exc:
	LOAD	t0, TI_TASK($28)
	 nop
	LOAD	t0, THREAD_BUADDR(t0)	# t0 is just past last good address
	 nop
	SUB	len, AT, t0		# len number of uncopied bytes
	/*
	 * Here's where we rely on src and dst being incremented in tandem,
	 *   See (3) above.
	 * dst += (fault addr - src) to put dst at first byte to clear
	 */
	ADD	dst, t0			# compute start address in a1
	SUB	dst, src
	/*
	 * Clear len bytes starting at dst.  Can't call __bzero because it
	 * might modify len.  An inefficient loop for these rare times...
	 */
	beqz	len, done
	 SUB	src, len, 1
1:	sb	zero, 0(dst)
	ADD	dst, dst, 1
	bnez	src, 1b
	 SUB	src, src, 1
	jr	ra
	 nop


#define SEXC(n)				\
s_exc_p ## n ## u:			\
	jr	ra;			\
	 ADD	len, len, n*NBYTES

SEXC(8)
SEXC(7)
SEXC(6)
SEXC(5)
SEXC(4)
SEXC(3)
SEXC(2)
SEXC(1)

s_exc_p1:
	jr	ra
	 ADD	len, len, 1
s_exc:
	jr	ra
	 nop

	.align	5
LEAF(memmove)
	ADD	t0, a0, a2
	ADD	t1, a1, a2
	sltu	t0, a1, t0			# dst + len <= src -> memcpy
	sltu	t1, a0, t1			# dst >= src + len -> memcpy
	and	t0, t1
	beqz	t0, __memcpy
	 move	v0, a0				/* return value */
	beqz	a2, r_out
	END(memmove)

	/* fall through to __rmemcpy */
LEAF(__rmemcpy)					/* a0=dst a1=src a2=len */
	 sltu	t0, a1, a0
	beqz	t0, r_end_bytes_up		# src >= dst
	 nop
	ADD	a0, a2				# dst = dst + len
	ADD	a1, a2				# src = src + len

r_end_bytes:
	lb	t0, -1(a1)
	SUB	a2, a2, 0x1
	sb	t0, -1(a0)
	SUB	a1, a1, 0x1
	bnez	a2, r_end_bytes
	 SUB	a0, a0, 0x1

r_out:
	jr	ra
	 move	a2, zero

r_end_bytes_up:
	lb	t0, (a1)
	SUB	a2, a2, 0x1
	sb	t0, (a0)
	ADD	a1, a1, 0x1
	bnez	a2, r_end_bytes_up
	 ADD	a0, a0, 0x1

	jr	ra
	 move	a2, zero
	END(__rmemcpy)

/*
 * lock.h
 *
 * Ethernet Packet Interface to NDIS drivers.
 *
 * Copyright 1998 Equivalence Pty. Ltd.
 *
 * Original code by William Ingle (address unknown)
 *
 * $Log: lock.h,v $
 * Revision 1.2  2005/08/14 09:58:24  dmitry
 * B25117. Merged pwlib version 1_9_0 from vendor branch.
 *
 * Revision 1.1.2.1  2005/08/14 09:53:08  dmitry
 * Added pwlib version 1.9.0 from pwlib-v1_9_0-src-tar.gz
 *
 * Revision 1.1  1998/09/28 08:08:40  robertj
 * Initial revision
 *
 */

DWORD _stdcall PacketPageLock  (DWORD lpMem, DWORD cbSize);
void  _stdcall PacketPageUnlock(void * lpMem, DWORD cbSize);


// End of File ////////////////////////////////////////////////////////////////

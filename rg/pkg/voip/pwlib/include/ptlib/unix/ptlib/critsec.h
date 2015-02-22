/*
 * critsec.h
 *
 * Critical section mutex class.
 *
 * Portable Windows Library
 *
 * Copyright (C) 2004 Post Increment
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Portable Windows Library.
 *
 * The Initial Developer of the Original Code is Post Increment
 *
 * Contributor(s): ______________________________________.
 *
 * $Log: critsec.h,v $
 * Revision 1.3  2006/04/26 10:15:34  olegy
 * B31918 - importing new 1.10.0 pwlib, 1.18.0 openh323 versions
 *
 * Revision 1.2  2005/08/14 09:56:34  dmitry
 * B25117. Merged pwlib version 1_9_0 from vendor branch.
 *
 * Revision 1.1.2.1  2005/08/14 09:50:57  dmitry
 * Added pwlib version 1.9.0 from pwlib-v1_9_0-src-tar.gz
 *
 * Revision 1.1.2.2  2006/04/16 11:40:13  olegy
 * B31918 - update pwlib version to 1.10.0
 *
 * Revision 1.4  2005/11/04 06:56:10  csoutheren
 * Added new class PSync as abstract base class for all mutex/sempahore classes
 * Changed PCriticalSection to use Wait/Signal rather than Enter/Leave
 * Changed Wait/Signal to be const member functions
 * Renamed PMutex to PTimedMutex and made PMutex synonym for PCriticalSection.
 * This allows use of very efficient mutex primitives in 99% of cases where timed waits
 * are not needed
 *
 * Revision 1.3  2004/07/11 07:56:36  csoutheren
 * Applied jumbo VxWorks patch, thanks to Eize Slange
 *
 * Revision 1.2  2004/04/18 12:37:40  csoutheren
 * Modified to detect sem_wait etc on Linux systems
 *
 * Revision 1.1  2004/04/11 03:02:07  csoutheren
 * Initial version
 *
 */

  // Unix specific critical section implementation
#if defined P_HAS_SEMAPHORES && !defined P_VXWORKS
  mutable sem_t sem;
#endif

// End Of File ///////////////////////////////////////////////////////////////

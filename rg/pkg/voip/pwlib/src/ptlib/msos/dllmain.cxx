/*
 * dllmain.cxx
 *
 * DLL main entry point for PTLib.dll and PWLib.dll
 *
 * Portable Windows Library
 *
 * Copyright (c) 1993-1998 Equivalence Pty. Ltd.
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
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Portions are Copyright (C) 1993 Free Software Foundation, Inc.
 * All Rights Reserved.
 *
 * Contributor(s): ______________________________________.
 *
 * $Log: dllmain.cxx,v $
 * Revision 1.3  2006/04/26 10:16:16  olegy
 * B31918 - importing new 1.10.0 pwlib, 1.18.0 openh323 versions
 *
 * Revision 1.2  2005/08/14 09:57:54  dmitry
 * B25117. Merged pwlib version 1_9_0 from vendor branch.
 *
 * Revision 1.1.2.1  2005/08/14 09:52:39  dmitry
 * Added pwlib version 1.9.0 from pwlib-v1_9_0-src-tar.gz
 *
 * Revision 1.1.2.2  2006/04/16 11:41:14  olegy
 * B31918 - update pwlib version to 1.10.0
 *
 * Revision 1.3  2005/09/18 13:01:43  dominance
 * fixed pragma warnings when building with gcc.
 *
 * Revision 1.2  2004/10/23 10:53:19  ykiryanov
 * Added ifdef _WIN32_WCE for PocketPC 2003 SDK port
 *
 * Revision 1.1  1998/09/25 11:04:09  craigs
 * Initial revision
 *
 */

#ifdef _MSC_VER
#pragma warning(disable:4201 4514)
#endif

#include <windows.h>

#ifndef _WIN32_WCE
HINSTANCE PDllInstance;
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID)
#else
HANDLE PDllInstance;
BOOL WINAPI DllMain(HANDLE hinstDLL, DWORD fdwReason, LPVOID)
#endif
{
  if (fdwReason == DLL_PROCESS_ATTACH)
    PDllInstance = hinstDLL;
  return TRUE;
}


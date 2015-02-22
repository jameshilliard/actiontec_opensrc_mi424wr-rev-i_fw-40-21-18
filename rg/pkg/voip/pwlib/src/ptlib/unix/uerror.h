/*
 * uerror.h
 *
 * Error structure for passing error messages between various classes.
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
 * $Log: uerror.h,v $
 * Revision 1.3  2005/08/14 09:58:00  dmitry
 * B25117. Merged pwlib version 1_9_0 from vendor branch.
 *
 * Revision 1.2.2.1  2005/08/14 09:52:45  dmitry
 * Added pwlib version 1.9.0 from pwlib-v1_9_0-src-tar.gz
 *
 * Revision 1.2  1998/09/24 04:12:28  robertj
 * Added open software license.
 *
 */

#ifndef PXERROR_H
#define PXERROR_H

struct PXErrorStruct {
  int  code;
  const char * str;
};

#endif

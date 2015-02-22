/*
 * custom.h
 *
 * PWLib application header file for $$PRODUCT_NAME$$
 *
 * Customisable application configurationfor OEMs.
 *
 * Copyright (c) $$YEAR$$ $$COPYRIGHT_HOLDER$$
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
 * Contributor(s): ______________________________________.
 *
 * $Log: custom.h,v $
 * Revision 1.2  2005/08/14 09:58:21  dmitry
 * B25117. Merged pwlib version 1_9_0 from vendor branch.
 *
 * Revision 1.1.2.1  2005/08/14 09:53:06  dmitry
 * Added pwlib version 1.9.0 from pwlib-v1_9_0-src-tar.gz
 *
 */

#include <ptclib/httpsvc.h>

enum {
  SkName, SkCompany, SkEMail,
  NumSecuredKeys
};


extern PHTTPServiceProcess::Info ProductInfo;


// End of File ///////////////////////////////////////////////////////////////

/*
 * custom.h
 *
 * PWLib application header file for xmlrpcsrvr
 *
 * Customisable application configurationfor OEMs.
 *
 * Copyright 2002 Equivalence
 *
 * $Log: custom.h,v $
 * Revision 1.2  2005/08/14 09:57:27  dmitry
 * B25117. Merged pwlib version 1_9_0 from vendor branch.
 *
 * Revision 1.1.2.1  2005/08/14 09:52:02  dmitry
 * Added pwlib version 1.9.0 from pwlib-v1_9_0-src-tar.gz
 *
 * Revision 1.1  2002/10/02 08:58:20  craigs
 * Initial version
 *
 */

#include <ptclib/httpsvc.h>

enum {
  SkName, SkCompany, SkEMail,
  NumSecuredKeys
};


extern PHTTPServiceProcess::Info ProductInfo;


// End of File ///////////////////////////////////////////////////////////////

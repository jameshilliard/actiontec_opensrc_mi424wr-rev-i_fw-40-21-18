/*
 * main.h
 *
 * PWLib application header file for PxmlTest
 *
 * Copyright 2002 David Iodice.
 *
 * $Log: main.h,v $
 * Revision 1.2  2005/08/14 09:57:17  dmitry
 * B25117. Merged pwlib version 1_9_0 from vendor branch.
 *
 * Revision 1.1.2.1  2005/08/14 09:51:46  dmitry
 * Added pwlib version 1.9.0 from pwlib-v1_9_0-src-tar.gz
 *
 * Revision 1.1  2002/03/07 01:56:56  robertj
 * Added XML sample/test program.
 *
 */

#ifndef _PxmlTest_MAIN_H
#define _PxmlTest_MAIN_H


#include <ptclib/pxml.h>


class PxmlTest : public PProcess
{
  PCLASSINFO(PxmlTest, PProcess)

  public:
    PxmlTest();
    void Main();
};

#endif  // _PxmlTest_MAIN_H


// End of File ///////////////////////////////////////////////////////////////

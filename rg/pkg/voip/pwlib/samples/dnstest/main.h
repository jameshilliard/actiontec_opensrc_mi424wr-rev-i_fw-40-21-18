/*
 * main.h
 *
 * PWLib application header file for DNSTest
 *
 * Copyright 2003 Equivalence
 *
 * $Log: main.h,v $
 * Revision 1.2  2005/08/14 09:57:05  dmitry
 * B25117. Merged pwlib version 1_9_0 from vendor branch.
 *
 * Revision 1.1.2.1  2005/08/14 09:51:33  dmitry
 * Added pwlib version 1.9.0 from pwlib-v1_9_0-src-tar.gz
 *
 * Revision 1.1  2003/04/15 04:12:38  craigs
 * Initial version
 *
 */

#ifndef _DNSTest_MAIN_H
#define _DNSTest_MAIN_H




class DNSTest : public PProcess
{
  PCLASSINFO(DNSTest, PProcess)

  public:
    DNSTest();
    void Main();
};


#endif  // _DNSTest_MAIN_H


// End of File ///////////////////////////////////////////////////////////////

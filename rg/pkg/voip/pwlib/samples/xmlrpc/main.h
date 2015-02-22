/*
 * main.h
 *
 * PWLib application header file for XMLRPCApp
 *
 * Copyright 2002 Equivalence
 *
 * $Log: main.h,v $
 * Revision 1.2  2005/08/14 09:57:26  dmitry
 * B25117. Merged pwlib version 1_9_0 from vendor branch.
 *
 * Revision 1.1.2.1  2005/08/14 09:52:00  dmitry
 * Added pwlib version 1.9.0 from pwlib-v1_9_0-src-tar.gz
 *
 * Revision 1.1  2002/03/26 07:05:28  craigs
 * Initial version
 *
 */

#ifndef _XMLRPCApp_MAIN_H
#define _XMLRPCApp_MAIN_H




class XMLRPCApp : public PProcess
{
  PCLASSINFO(XMLRPCApp, PProcess)

  public:
    XMLRPCApp();
    void Main();
};


#endif  // _XMLRPCApp_MAIN_H


// End of File ///////////////////////////////////////////////////////////////

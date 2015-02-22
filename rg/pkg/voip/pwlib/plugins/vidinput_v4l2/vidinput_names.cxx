/*
 * vidinput_names.cxx
 *
 * Classes to support streaming video input (grabbing) and output.
 *
 * Portable Windows Library
 *
 * Copyright (c) 1993-2000 Equivalence Pty. Ltd.
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
 * Contributor(s): Derek Smithies (derek@indranet.co.nz)
 *                 Mark Cooke (mpc@star.sr.bham.ac.uk)
 *                 Nicola Orru' <nigu@itadinanta.it>
 *
 * $Log: vidinput_names.cxx,v $
 * Revision 1.3  2006/04/26 10:15:46  olegy
 * B31918 - importing new 1.10.0 pwlib, 1.18.0 openh323 versions
 *
 * Revision 1.2  2005/08/14 09:57:02  dmitry
 * B25117. Merged pwlib version 1_9_0 from vendor branch.
 *
 * Revision 1.1.2.1  2005/08/14 09:51:30  dmitry
 * Added pwlib version 1.9.0 from pwlib-v1_9_0-src-tar.gz
 *
 * Revision 1.1.2.2  2006/04/16 11:40:31  olegy
 * B31918 - update pwlib version to 1.10.0
 *
 * Revision 1.4  2006/01/07 16:09:58  dsandras
 * Removed name duplication for now.
 *
 * Revision 1.3  2005/11/30 12:47:39  csoutheren
 * Removed tabs, reformatted some code, and changed tags for Doxygen
 *
 * Revision 1.2  2004/11/07 22:48:47  dominance
 * fixed copyright of v4l2 plugin. Last commit's credits go to Nicola Orru' <nigu@itadinanta.it> ...
 *
 */
#include "vidinput_names.h"

void  V4LXNames::ReadDeviceDirectory(PDirectory devdir, POrdinalToString & vid)
{
  if (!devdir.Open())
    return;

  do {
    PString filename = devdir.GetEntryName();
    PString devname = devdir + filename;
    if (devdir.IsSubDir())
      ReadDeviceDirectory(devname, vid);
    else {

      PFileInfo info;
      if (devdir.GetInfo(info) && info.type == PFileInfo::CharDevice) {
        struct stat s;
        if (lstat(devname, &s) == 0) {
 
          static const int deviceNumbers[] = { 81 };
          for (PINDEX i = 0; i < PARRAYSIZE(deviceNumbers); i++) {
            if (MAJOR(s.st_rdev) == deviceNumbers[i]) {
              PINDEX num = MINOR(s.st_rdev);
              if (num <= 63 && num >= 0) {
                vid.SetAt(num, devname);
              }
            }
          }
        }
      }
    }
  } while (devdir.Next());
}

void V4LXNames::PopulateDictionary()
{
  PINDEX i, j;
  PStringToString tempList;

  for (i = 0; i < inputDeviceNames.GetSize(); i++) {
    PString ufname = BuildUserFriendly(inputDeviceNames[i]);
    tempList.SetAt(inputDeviceNames[i], ufname);
  }

  //Now, we need to cope with the case where there are two video
  //devices available, which both have the same user friendly name.
  //Matching user friendly names have a (X) appended to the name.
  for (i = 0; i < tempList.GetSize(); i++) {
    PString userName = tempList.GetDataAt(i); 

    PINDEX matches = 1;    
    for (j = i + 1; j < tempList.GetSize(); j++) {
      if (tempList.GetDataAt(j) == userName) {
        matches++;
        PStringStream revisedUserName;
        revisedUserName << userName << " (" << matches << ")";
        tempList.SetDataAt(j, revisedUserName);
      }
    }
  }

  //At this stage, we have correctly modified the temp list of names.
  for (j = 0; j < tempList.GetSize(); j++)
    AddUserDeviceName(tempList.GetDataAt(j), tempList.GetKeyAt(j));  
}

PString V4LXNames::GetUserFriendly(PString devName)
{
  PWaitAndSignal m(mutex);

  
  PString result= deviceKey(devName);
  if (result.IsEmpty())
    return devName;

  return result;
}

PString V4LXNames::GetDeviceName(PString userName)
{
  PWaitAndSignal m(mutex);

  for (PINDEX i = 0; i < userKey.GetSize(); i++)
    if (userKey.GetKeyAt(i).Find(userName) != P_MAX_INDEX)
      return userKey.GetDataAt(i);

  return userName;
}

void V4LXNames::AddUserDeviceName(PString userName, PString devName)
{
  if (userName != devName) { // must be a real userName!
    userKey.SetAt(userName, devName);
    deviceKey.SetAt(devName, userName);
  } else { // we didn't find a good userName
    if (!deviceKey.Contains (devName)) { // never met before: fallback
      userKey.SetAt(userName, devName);
      deviceKey.SetAt(devName, userName);
    } // no else: we already know the pair
  }
}


PStringList V4LXNames::GetInputDeviceNames()
{
  PWaitAndSignal m(mutex);
  PStringList result;
  for (PINDEX i = 0; i < inputDeviceNames.GetSize(); i++) {
    result += GetUserFriendly (inputDeviceNames[i]);
  }
  return result;
}

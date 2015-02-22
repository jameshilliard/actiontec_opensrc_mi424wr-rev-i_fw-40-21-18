/*
 * h235auth.h
 *
 * H.235 authorisation PDU's
 *
 * Open H323 Library
 *
 * Copyright (c) 1998-2001 Equivalence Pty. Ltd.
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
 * The Original Code is Open H323 Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): Fürbass Franz <franz.fuerbass@infonova.at>
 *
 * $Log: h235auth.h,v $
 * Revision 1.3  2006/04/26 08:40:30  olegy
 * B31918 - new openh323 version (1_18_0)
 *
 *
 * Revision 1.1.2.2  2006/04/16 16:11:13  olegy
 * B31918 - importing new version of openh323 1.18.0
 *
 * Revision 1.20  2005/11/30 13:05:01  csoutheren
 * Changed tags for Doxygen
 *
 * Revision 1.19  2005/02/13 23:54:48  csoutheren
 * Allow access to H.235 timestamp grace period
 * Thanks to Jan Willamowius
 *
 * Revision 1.18  2004/11/20 22:00:48  csoutheren
 * Added hacks for linker problem
 *
 * Revision 1.17  2004/11/12 06:04:42  csoutheren
 * Changed H235Authentiators to use PFactory
 *
 * Revision 1.16  2004/05/13 02:26:13  dereksmithies
 * Fixes so make docs does not generate warning messages about brackets.
 *
 * Revision 1.15  2003/04/30 00:28:50  robertj
 * Redesigned the alternate credentials in ARQ system as old implementation
 *   was fraught with concurrency issues, most importantly it can cause false
 *   detection of replay attacks taking out an endpoint completely.
 *
 * Revision 1.14  2003/04/01 04:47:48  robertj
 * Abstracted H.225 RAS transaction processing (RIP and secondary thread) in
 *   server environment for use by H.501 peer elements.
 *
 * Revision 1.13  2003/02/25 06:48:14  robertj
 * More work on PDU transaction abstraction.
 *
 * Revision 1.12  2003/02/11 04:43:22  robertj
 * Fixed use of asymmetrical authentication schemes such as MD5.
 *
 * Revision 1.11  2003/02/01 13:31:14  robertj
 * Changes to support CAT authentication in RAS.
 *
 * Revision 1.10  2003/01/08 04:40:31  robertj
 * Added more debug tracing for H.235 authenticators.
 *
 * Revision 1.9  2002/09/16 01:14:15  robertj
 * Added #define so can select if #pragma interface/implementation is used on
 *   platform basis (eg MacOS) rather than compiler, thanks Robert Monaghan.
 *
 * Revision 1.8  2002/09/03 06:19:36  robertj
 * Normalised the multi-include header prevention ifdef/define symbol.
 *
 * Revision 1.7  2002/08/05 10:03:47  robertj
 * Cosmetic changes to normalise the usage of pragma interface/implementation.
 *
 * Revision 1.6  2002/08/05 05:17:37  robertj
 * Fairly major modifications to support different authentication credentials
 *   in ARQ to the logged in ones on RRQ. For both client and server.
 * Various other H.235 authentication bugs and anomalies fixed on the way.
 *
 * Revision 1.5  2002/05/17 03:39:28  robertj
 * Fixed problems with H.235 authentication on RAS for server and client.
 *
 * Revision 1.4  2001/12/06 06:44:42  robertj
 * Removed "Win32 SSL xxx" build configurations in favour of system
 *   environment variables to select optional libraries.
 *
 * Revision 1.3  2001/09/14 00:13:37  robertj
 * Fixed problem with some athenticators needing extra conditions to be
 *   "active", so make IsActive() virtual and add localId to H235AuthSimpleMD5
 *
 * Revision 1.2  2001/09/13 01:15:18  robertj
 * Added flag to H235Authenticator to determine if gkid and epid is to be
 *   automatically set as the crypto token remote id and local id.
 *
 * Revision 1.1  2001/08/10 11:03:49  robertj
 * Major changes to H.235 support in RAS to support server.
 *
 */

#ifndef __OPAL_H235AUTH_H
#define __OPAL_H235AUTH_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

class H323TransactionPDU;
class H225_CryptoH323Token;
class H225_ArrayOf_AuthenticationMechanism;
class H225_ArrayOf_PASN_ObjectId;
class H235_ClearToken;
class H235_AuthenticationMechanism;
class PASN_ObjectId;
class PASN_Sequence;
class PASN_Array;

namespace PWLibStupidLinkerHacks {
extern int h235AuthLoader;
};

/** This abtract class embodies an H.235 authentication mechanism.
    NOTE: descendants must have a Clone() function for correct operation.
*/
class H235Authenticator : public PObject
{
    PCLASSINFO(H235Authenticator, PObject);
  public:
    H235Authenticator();

    virtual void PrintOn(
      ostream & strm
    ) const;

    virtual const char * GetName() const = 0;

    virtual BOOL PrepareTokens(
      PASN_Array & clearTokens,
      PASN_Array & cryptoTokens
    );

    virtual H235_ClearToken * CreateClearToken();
    virtual H225_CryptoH323Token * CreateCryptoToken();

    virtual BOOL Finalise(
      PBYTEArray & rawPDU
    );

    enum ValidationResult {
      e_OK = 0,     ///< Security parameters and Msg are ok, no security attacks
      e_Absent,     ///< Security parameters are expected but absent
      e_Error,      ///< Security parameters are present but incorrect
      e_InvalidTime,///< Security parameters indicate peer has bad real time clock
      e_BadPassword,///< Security parameters indicate bad password in token
      e_ReplyAttack,///< Security parameters indicate an attack was made
      e_Disabled    ///< Security is disabled by local system
    };

    virtual ValidationResult ValidateTokens(
      const PASN_Array & clearTokens,
      const PASN_Array & cryptoTokens,
      const PBYTEArray & rawPDU
    );

    virtual ValidationResult ValidateClearToken(
      const H235_ClearToken & clearToken
    );

    virtual ValidationResult ValidateCryptoToken(
      const H225_CryptoH323Token & cryptoToken,
      const PBYTEArray & rawPDU
    );

    virtual BOOL IsCapability(
      const H235_AuthenticationMechanism & mechansim,
      const PASN_ObjectId & algorithmOID
    ) = 0;

    virtual BOOL SetCapability(
      H225_ArrayOf_AuthenticationMechanism & mechansims,
      H225_ArrayOf_PASN_ObjectId & algorithmOIDs
    ) = 0;

    virtual BOOL UseGkAndEpIdentifiers() const;

    virtual BOOL IsSecuredPDU(
      unsigned rasPDU,
      BOOL received
    ) const;

    virtual BOOL IsActive() const;

    virtual void Enable(
      BOOL enab = TRUE
    ) { enabled = enab; }
    virtual void Disable() { enabled = FALSE; }

    virtual const PString & GetRemoteId() const { return remoteId; }
    virtual void SetRemoteId(const PString & id) { remoteId = id; }

    virtual const PString & GetLocalId() const { return localId; }
    virtual void SetLocalId(const PString & id) { localId = id; }

    virtual const PString & GetPassword() const { return password; }
    virtual void SetPassword(const PString & pw) { password = pw; }

    virtual int GetTimestampGracePeriod() const { return timestampGracePeriod; }
    virtual void SetTimestampGracePeriod(int grace) { timestampGracePeriod = grace; }

  protected:
    BOOL AddCapability(
      unsigned mechanism,
      const PString & oid,
      H225_ArrayOf_AuthenticationMechanism & mechansims,
      H225_ArrayOf_PASN_ObjectId & algorithmOIDs
    );

    BOOL     enabled;

    PString  remoteId;      // ID of remote entity
    PString  localId;       // ID of local entity
    PString  password;      // shared secret

    unsigned sentRandomSequenceNumber;
    unsigned lastRandomSequenceNumber;
    unsigned lastTimestamp;
    int      timestampGracePeriod;

    PMutex mutex;
};


PDECLARE_LIST(H235Authenticators, H235Authenticator)
#ifdef DOC_PLUS_PLUS
{
#endif
  public:
    void PreparePDU(
      H323TransactionPDU & pdu,
      PASN_Array & clearTokens,
      unsigned clearOptionalField,
      PASN_Array & cryptoTokens,
      unsigned cryptoOptionalField
    ) const;

    H235Authenticator::ValidationResult ValidatePDU(
      const H323TransactionPDU & pdu,
      const PASN_Array & clearTokens,
      unsigned clearOptionalField,
      const PASN_Array & cryptoTokens,
      unsigned cryptoOptionalField,
      const PBYTEArray & rawPDU
    ) const;
};




/** This class embodies a simple MD5 based authentication.
    The users password is concatenated with the 4 byte timestamp and 4 byte
    random fields and an MD5 generated and sent/verified
*/
class H235AuthSimpleMD5 : public H235Authenticator
{
    PCLASSINFO(H235AuthSimpleMD5, H235Authenticator);
  public:
    H235AuthSimpleMD5();

    PObject * Clone() const;

    virtual const char * GetName() const;

    virtual H225_CryptoH323Token * CreateCryptoToken();

    virtual ValidationResult ValidateCryptoToken(
      const H225_CryptoH323Token & cryptoToken,
      const PBYTEArray & rawPDU
    );

    virtual BOOL IsCapability(
      const H235_AuthenticationMechanism & mechansim,
      const PASN_ObjectId & algorithmOID
    );

    virtual BOOL SetCapability(
      H225_ArrayOf_AuthenticationMechanism & mechansim,
      H225_ArrayOf_PASN_ObjectId & algorithmOIDs
    );

    virtual BOOL IsSecuredPDU(
      unsigned rasPDU,
      BOOL received
    ) const;
};


/** This class embodies a RADIUS compatible based authentication (aka Cisco
    Access Token or CAT).
    The users password is concatenated with the 4 byte timestamp and 1 byte
    random fields and an MD5 generated and sent/verified via the challenge
    field.
*/
class H235AuthCAT : public H235Authenticator
{
    PCLASSINFO(H235AuthCAT, H235Authenticator);
  public:
    H235AuthCAT();

    PObject * Clone() const;

    virtual const char * GetName() const;

    virtual H235_ClearToken * CreateClearToken();

    virtual ValidationResult ValidateClearToken(
      const H235_ClearToken & clearToken
    );

    virtual BOOL IsCapability(
      const H235_AuthenticationMechanism & mechansim,
      const PASN_ObjectId & algorithmOID
    );

    virtual BOOL SetCapability(
      H225_ArrayOf_AuthenticationMechanism & mechansim,
      H225_ArrayOf_PASN_ObjectId & algorithmOIDs
    );

    virtual BOOL IsSecuredPDU(
      unsigned rasPDU,
      BOOL received
    ) const;
};


#if P_SSL

namespace PWLibStupidLinkerHacks {
extern int h235AuthProcedure1Loader;
};

/** This class embodies the H.235 "base line Procedure 1" from Annex D.
*/
class H235AuthProcedure1 : public H235Authenticator
{
    PCLASSINFO(H235AuthProcedure1, H235Authenticator);
  public:
    H235AuthProcedure1();

    PObject * Clone() const;

    virtual const char * GetName() const;

    virtual H225_CryptoH323Token * CreateCryptoToken();

    virtual BOOL Finalise(
      PBYTEArray & rawPDU
    );

    virtual ValidationResult ValidateCryptoToken(
      const H225_CryptoH323Token & cryptoToken,
      const PBYTEArray & rawPDU
    );

    virtual BOOL IsCapability(
      const H235_AuthenticationMechanism & mechansim,
      const PASN_ObjectId & algorithmOID
    );

    virtual BOOL SetCapability(
      H225_ArrayOf_AuthenticationMechanism & mechansim,
      H225_ArrayOf_PASN_ObjectId & algorithmOIDs
    );

    virtual BOOL UseGkAndEpIdentifiers() const;
};

#endif


#endif //__OPAL_H235AUTH_H


/////////////////////////////////////////////////////////////////////////////

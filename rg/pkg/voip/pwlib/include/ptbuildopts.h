#ifndef _PT_BUILDOPTS_H
#define _PT_BUILDOPTS_H


/////////////////////////////////////////////////
//
// host type
//

#define	P_LINUX "2.4.24-jungo-3"
/* #undef	P_FREEBSD */
/* #undef	P_OPENBSD */
/* #undef	P_NETBSD */
/* #undef	P_SOLARIS */
/* #undef	P_MACOSX */
/* #undef	P_CYGWIN */
/* #undef	P_UNKNOWN_OS */

/////////////////////////////////////////////////
//
// Processor endianess
//

#define PBYTE_ORDER PLITTLE_ENDIAN


/////////////////////////////////////////////////
//
// IP v6 Support
//

#ifndef _WIN32_WCE
/* #undef P_HAS_IPV6 */
#endif //  _WIN32_WCE

#if defined(_MSC_VER) && P_HAS_IPV6

#pragma include_alias(<winsock2.h>, <@IPV6_DIR@/winsock2.h>)
#pragma include_alias(<ws2tcpip.h>, <@IPV6_DIR@/ws2tcpip.h>)

#define P_WINSOCK2_LIBRARY "ws2_32.lib"

#endif


/////////////////////////////////////////////////
//
// QoS Support
//
//

#ifndef _WIN32_WCE
#define P_HAS_QOS 0
#endif //  _WIN32_WCE

#if defined(_MSC_VER) && P_HAS_QOS

#pragma include_alias(<qossp.h>, <@QOS_DIR@/qossp.h>)

#define P_WINSOCK2_LIBRARY "ws2_32.lib"

#endif


/////////////////////////////////////////////////
//
// OpenSSL library for secure sockets layer
//

/* #undef P_SSL */

#if defined(_MSC_VER) && P_SSL

#pragma include_alias(<openssl/ssl.h>,        <@SSL_DIR@/inc32/openssl/ssl.h>)
#pragma include_alias(<openssl/safestack.h>,  <@SSL_DIR@/inc32/openssl/safestack.h>)
#pragma include_alias(<openssl/stack.h>,      <@SSL_DIR@/inc32/openssl/stack.h>)
#pragma include_alias(<openssl/crypto.h>,     <@SSL_DIR@/inc32/openssl/crypto.h>)
#pragma include_alias(<openssl/opensslv.h>,   <@SSL_DIR@/inc32/openssl/opensslv.h>)
#pragma include_alias(<openssl/lhash.h>,      <@SSL_DIR@/inc32/openssl/lhash.h>)
#pragma include_alias(<openssl/buffer.h>,     <@SSL_DIR@/inc32/openssl/buffer.h>)
#pragma include_alias(<openssl/bio.h>,        <@SSL_DIR@/inc32/openssl/bio.h>)
#pragma include_alias(<openssl/pem.h>,        <@SSL_DIR@/inc32/openssl/pem.h>)
#pragma include_alias(<openssl/evp.h>,        <@SSL_DIR@/inc32/openssl/evp.h>)
#pragma include_alias(<openssl/md2.h>,        <@SSL_DIR@/inc32/openssl/md2.h>)
#pragma include_alias(<openssl/opensslconf.h>,<@SSL_DIR@/inc32/openssl/opensslconf.h>)
#pragma include_alias(<openssl/md5.h>,        <@SSL_DIR@/inc32/openssl/md5.h>)
#pragma include_alias(<openssl/sha.h>,        <@SSL_DIR@/inc32/openssl/sha.h>)
#pragma include_alias(<openssl/ripemd.h>,     <@SSL_DIR@/inc32/openssl/ripemd.h>)
#pragma include_alias(<openssl/des.h>,        <@SSL_DIR@/inc32/openssl/des.h>)
#pragma include_alias(<openssl/e_os2.h>,      <@SSL_DIR@/inc32/openssl/e_os2.h>)
#pragma include_alias(<openssl/rc4.h>,        <@SSL_DIR@/inc32/openssl/rc4.h>)
#pragma include_alias(<openssl/rc2.h>,        <@SSL_DIR@/inc32/openssl/rc2.h>)
#pragma include_alias(<openssl/rc5.h>,        <@SSL_DIR@/inc32/openssl/rc5.h>)
#pragma include_alias(<openssl/blowfish.h>,   <@SSL_DIR@/inc32/openssl/blowfish.h>)
#pragma include_alias(<openssl/cast.h>,       <@SSL_DIR@/inc32/openssl/cast.h>)
#pragma include_alias(<openssl/idea.h>,       <@SSL_DIR@/inc32/openssl/idea.h>)
#pragma include_alias(<openssl/mdc2.h>,       <@SSL_DIR@/inc32/openssl/mdc2.h>)
#pragma include_alias(<openssl/rsa.h>,        <@SSL_DIR@/inc32/openssl/rsa.h>)
#pragma include_alias(<openssl/bn.h>,         <@SSL_DIR@/inc32/openssl/bn.h>)
#pragma include_alias(<openssl/dsa.h>,        <@SSL_DIR@/inc32/openssl/dsa.h>)
#pragma include_alias(<openssl/dh.h>,         <@SSL_DIR@/inc32/openssl/dh.h>)
#pragma include_alias(<openssl/objects.h>,    <@SSL_DIR@/inc32/openssl/objects.h>)
#pragma include_alias(<openssl/asn1.h>,       <@SSL_DIR@/inc32/openssl/asn1.h>)
#pragma include_alias(<openssl/x509.h>,       <@SSL_DIR@/inc32/openssl/x509.h>)
#pragma include_alias(<openssl/x509_vfy.h>,   <@SSL_DIR@/inc32/openssl/x509_vfy.h>)
#pragma include_alias(<openssl/pkcs7.h>,      <@SSL_DIR@/inc32/openssl/pkcs7.h>)
#pragma include_alias(<openssl/pem2.h>,       <@SSL_DIR@/inc32/openssl/pem2.h>)
#pragma include_alias(<openssl/ssl2.h>,       <@SSL_DIR@/inc32/openssl/ssl2.h>)
#pragma include_alias(<openssl/ssl3.h>,       <@SSL_DIR@/inc32/openssl/ssl3.h>)
#pragma include_alias(<openssl/tls1.h>,       <@SSL_DIR@/inc32/openssl/tls1.h>)
#pragma include_alias(<openssl/ssl23.h>,      <@SSL_DIR@/inc32/openssl/ssl23.h>)
#pragma include_alias(<openssl/err.h>,        <@SSL_DIR@/inc32/openssl/err.h>)
#pragma include_alias(<openssl/rand.h>,       <@SSL_DIR@/inc32/openssl/rand.h>)
#pragma include_alias(<openssl/symhacks.h>,   <@SSL_DIR@/inc32/openssl/symhacks.h>)
#pragma include_alias(<openssl/comp.h>,       <@SSL_DIR@/inc32/openssl/comp.h>)
#pragma include_alias(<openssl/ossl_typ.h>,   <@SSL_DIR@/inc32/openssl/ossl_typ.h>)
#pragma include_alias(<openssl/md4.h>,        <@SSL_DIR@/inc32/openssl/md4.h>)
#pragma include_alias(<openssl/des_old.h>,    <@SSL_DIR@/inc32/openssl/des_old.h>)
#pragma include_alias(<openssl/ui_compat.h>,  <@SSL_DIR@/inc32/openssl/ui_compat.h>)
#pragma include_alias(<openssl/ui.h>,         <@SSL_DIR@/inc32/openssl/ui.h>)
#pragma include_alias(<openssl/aes.h>,        <@SSL_DIR@/inc32/openssl/aes.h>)
#pragma include_alias(<openssl/obj_mac.h>,    <@SSL_DIR@/inc32/openssl/obj_mac.h>)
#pragma include_alias(<openssl/kssl.h>,       <@SSL_DIR@/inc32/openssl/kssl.h>)

#ifdef _DEBUG
#define P_SSL_LIBDIR "out32.dbg"
#else
#define P_SSL_LIBDIR "out32"
#endif

#define P_SSL_LIB1 "@SSL_DIR@/" P_SSL_LIBDIR "/ssleay32.lib"
#define P_SSL_LIB2 "@SSL_DIR@/" P_SSL_LIBDIR "/libeay32.lib"

#endif


/////////////////////////////////////////////////
//
// EXPAT library for XML parsing
//

/* #undef P_EXPAT */

#if defined(_MSC_VER) && P_EXPAT

#pragma include_alias(<expat.h>, <@EXPAT_DIR@/lib/expat.h>)

#ifdef _DEBUG
#define P_EXPAT_LIBDIR "Debug"
#else
#define P_EXPAT_LIBDIR "Release"
#endif

#define P_EXPAT_LIBRARY "@EXPAT_DIR@/" P_EXPAT_LIBDIR "/expat.lib"

#endif


/////////////////////////////////////////////////
//
// OpenLDAP
//

/* #undef P_LDAP */

#if defined(_MSC_VER) && P_LDAP

#pragma include_alias(<ldap.h>,          <@LDAP_DIR@/include/ldap.h>)
#pragma include_alias(<lber.h>,          <@LDAP_DIR@/include/lber.h>)
#pragma include_alias(<lber_types.h>,    <@LDAP_DIR@/include/lber_types.h>)
#pragma include_alias(<ldap_features.h>, <@LDAP_DIR@/include/ldap_features.h>)
#pragma include_alias(<ldap_cdefs.h>,    <@LDAP_DIR@/include/ldap_cdefs.h>)

#ifdef _DEBUG
#define P_LDAP_LIBRARY "@LDAP_DIR@/DLLDebug/openldapd.lib"
#else
#define P_LDAP_LIBRARY "@LDAP_DIR@/DLLRelease/openldap.lib"
#endif

#endif


/////////////////////////////////////////////////
//
// DNS resolver
//

/* #undef P_DNS */

#if defined(_MSC_VER) && P_DNS

#pragma include_alias(<windns.h>, <@DNS_DIR@/Include/windns.h>)

#define P_DNS_LIBRARY "@DNS_DIR@/Lib/DnsAPI.Lib"

#endif



/////////////////////////////////////////////////
//
// SAPI speech API (Windows only)
//

/* #undef P_SAPI */

#if defined(_MSC_VER) && P_SAPI

#pragma include_alias(<sphelper.h>, <@SAPI_DIR@/include/sphelper.h>)
#pragma include_alias(<sapi.h>, <@SAPI_DIR@/include/sapi.h>)
#pragma include_alias(<sapiddk.h>, <@SAPI_DIR@/include/sapiddk.h>)
#pragma include_alias(<SPError.h>, <@SAPI_DIR@/include/SPError.h>)
#pragma include_alias(<SPDebug.h>, <@SAPI_DIR@/include/SPDebug.h>)

#define P_SAPI_LIBRARY "@SAPI_DIR@/Lib/i386/sapi.lib"

#endif



/////////////////////////////////////////////////
//
// Cyrus SASL
//

/* #undef P_SASL */
/* #undef P_SASL2 */
/* #undef P_HAS_SASL_SASL_H */

#if defined(_MSC_VER) && (defined(P_SASL) || defined(P_SASL2))

#pragma include_alias(<sasl/sasl.h>, <@SASL_DIR@/include/sasl.h>)

#define P_SASL_LIBRARY "@SASL_DIR@/lib/libsasl.lib"
#define P_HAS_SASL_SASL_H 1

#endif



/////////////////////////////////////////////////
//
// SDL toolkit
//

#define P_SDL 0

#if defined(_MSC_VER) && P_SDL
#pragma include_alias(<SDL/SDL.h>, <@SDL_DIR@/include/SDL.h>)

/* #undef P_SDL_LIBDIR */

#ifndef P_SDL_LIBDIR
 #ifdef _DEBUG
  #define P_SDL_LIBDIR "VisualC/SDL/Debug"
 #else
  #define P_SDL_LIBDIR "VisualC/SDL/Release"
 #endif
#endif

#define P_SDL_LIBRARY "@SDL_DIR@/" P_SDL_LIBDIR "/SDL.lib"

#endif


/////////////////////////////////////////////////
//
// Runtime dynamic link libraries
//

#define P_DYNALINK 1
/* #undef P_HAS_PLUGINS */

/////////////////////////////////////////////////
//
// Regex library
//

#define P_REGEX 1

/////////////////////////////////////////////////
//
// various non-core functions
//

/* #undef P_TTS */
/* #undef P_ASN */
/* #undef P_STUN */
/* #undef P_PIPECHAN */
/* #undef P_DTMF */
/* #undef P_WAVFILE */
/* #undef P_SOCKS */
/* #undef P_FTP */
/* #undef P_SNMP */
/* #undef P_TELNET */
/* #undef P_REMCONN */
/* #undef P_SERIAL */
/* #undef P_POP3SMTP */
#define P_AUDIO
/* #undef P_ALSA */
/* #undef P_VIDEO */
#define NO_VIDEO_CAPTURE 

/* #undef P_VXML */
/* #undef P_JABBER */
/* #undef P_XMLRPC */
/* #undef P_SOAP */
/* #undef P_HTTPSVC */
/* #undef P_HTTP */
/* #undef P_CONFIG_FILE */

/////////////////////////////////////////////////
//
// PThreads and related vars
//

#define P_PTHREADS 1
#define P_HAS_SEMAPHORES 1
#define P_PTHREADS_XPG6 1
#define P_HAS_SEMAPHORES_XPG6 1

/////////////////////////////////////////////////
//
// DCOM Support (Windows only)
//

#if defined(_MSC_VER)
  #define _WIN32_DCOM 1
  #define _OLE_LIB "ole32.lib"
#endif

/////////////////////////////////////////////////
//
// various functions
//
#define USE_SYSTEM_SWAB 

#define	PWLIB_MAJOR 1
#define	PWLIB_MINOR 9
#define	PWLIB_BUILD 0
#define	PWLIB_VERSION "1.9.0"

/* #undef	P_64BIT */
#define	PHAS_TEMPLATES 1
/* #undef	PNO_LONG_DOUBLE */
#define	P_HAS_POSIX_READDIR_R 3
#define  P_HAS_STL_STREAMS 1
/* #undef	P_HAS_ATOMIC_INT */
#define  P_HAS_RECURSIVE_MUTEX 1
#define  P_NEEDS_GNU_CXX_NAMESPACE 0
/* #undef  PMEMORY_CHECK */
#define  P_HAS_RECVMSG 1

#endif // _PT_BUILDOPTS_H


// End Of File ///////////////////////////////////////////////////////////////

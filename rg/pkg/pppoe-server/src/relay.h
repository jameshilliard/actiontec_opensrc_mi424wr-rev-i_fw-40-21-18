/**********************************************************************
*
* relay.h
*
* Definitions for PPPoE relay
*
* Copyright (C) 2001 Roaring Penguin Software Inc.
*
* This program may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
* LIC: GPL
*
*
***********************************************************************/

#include <linux/if_ether.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include "pppoe.h"

typedef struct pppoe_if_t pppoe_if_t;

/* Session state for relay */
struct SessionHashStruct;
typedef struct SessionStruct {
    struct SessionStruct *next;	/* Free list link */
    struct SessionStruct *prev;	/* Free list link */
    struct SessionHashStruct *acHash; /* Hash bucket for AC MAC/Session */
    struct SessionHashStruct *clientHash; /* Hash bucket for client MAC/Session */
    unsigned long epoch;		/* Epoch when last activity was seen */
    UINT16_t sesNum;		/* Session number assigned by relay */
} PPPoESession;

/* Hash table entry to find sessions */
typedef struct SessionHashStruct {
    struct SessionHashStruct *next; /* Link in hash chain */
    struct SessionHashStruct *prev; /* Link in hash chain */
    struct SessionHashStruct *peer; /* Peer for this session */
    struct net_device *interface;   /* Interface */
    unsigned char peerMac[ETH_ALEN]; /* Peer's MAC address */
    UINT16_t sesNum;		/* Session number */
    PPPoESession *ses;		/* Session data */
} SessionHash;

/* Function prototypes */

/* Function prototypes */
unsigned int hash(unsigned char const *mac, UINT16_t sesNum);
void addHash(SessionHash *sh);
void unhash(SessionHash *sh);

int relayHandlePADT(pppoe_if_t *iface, struct sk_buff *packet);
int relayHandlePADI(pppoe_if_t *iface, struct sk_buff *packet);
int relayHandlePADO(pppoe_if_t *iface, struct sk_buff *packet);
int relayHandlePADR(pppoe_if_t *iface, struct sk_buff *packet);
int relayHandlePADS(pppoe_if_t *iface, struct sk_buff *packet);
void relaySendError(unsigned char code,
		    UINT16_t session,
		    struct net_device *iface,
		    unsigned char const *mac,
		    PPPoETag const *hostUniq,
		    char const *errMsg);
int relayGotDiscoveryPacket(pppoe_if_t *iface, struct sk_buff *m);
int relayGotSessionPacket(pppoe_if_t *iface, struct sk_buff *m);

int pppoe_relay_session_exists(struct sk_buff *m);
pppoe_if_t *pppoe_relay_dev_get(struct net_device *dev);
void relayInterfacesClear(void);
int relayAddInterface(struct net_device *dev, int clientOK, int acOK);
void relayRemoveInterface(struct net_device *dev);

unsigned long cleanSessions(unsigned long timeout);
#define DEFAULT_SESSIONS 100

/* Hash table size -- a prime number; gives load factor of around 6
   for 65534 sessions */
#define HASHTAB_SIZE 18917

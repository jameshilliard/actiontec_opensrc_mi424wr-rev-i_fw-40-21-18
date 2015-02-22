/***********************************************************************
*
* relay.c
*
* Implementation of PPPoE relay
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

#define _GNU_SOURCE 1 /* For SA_RESTART */

#include <linux/module.h>
#include "relay.h"

/* This is a node in the interfaces linked list */
struct pppoe_if_t {
    struct pppoe_if_t *next;
    struct net_device *dev;
    int acOK; /* true if this interface should relay PADO, PADS packets. */
    int clientOK; /* true if this interface should relay PADI, PADR packets. */
};

/* Head of the interface linked list */
static pppoe_if_t *if_head;

/* Relay info */
int NumSessions;
int MaxSessions;
PPPoESession *AllSessions;
PPPoESession *FreeSessions;
PPPoESession *ActiveSessions;

SessionHash *AllHashes;
SessionHash *FreeHashes;
SessionHash *Buckets[HASHTAB_SIZE];

#define MTOD(m, t) ((t)((m)->data))
#define DEV_MAC(dev) ((dev)->dev_addr)

#ifdef DEBUG
#define PRINTK_DBG(fromat...) printk(KERN_DEBUG format)
#else
#define PRINTK_DBG(fromat...)
#endif

/* Our relay: Jungo Magic, if_index and peer_mac */
#define MY_RELAY_TAG_LEN (sizeof(u32) + sizeof(int) + ETH_ALEN)

#define JUNGO_RELAY_MAGIC 0xA0C00C1E

__inline void relay_enqueue(struct net_device *dst_ifx, struct sk_buff *m)
{
    m->dev = dst_ifx;
    dev_queue_xmit(m);
}

/**********************************************************************
*%FUNCTION: findTag (imported from common.c)
*%ARGUMENTS:
* packet -- the PPPoE discovery packet to parse
* type -- the type of the tag to look for
* tag -- will be filled in with tag contents
*%RETURNS:
* A pointer to the tag if one of the specified type is found; NULL
* otherwise.
*%DESCRIPTION:
* Looks for a specific tag type.
***********************************************************************/
unsigned char *
findTag(PPPoEPacket *packet, UINT16_t type, PPPoETag *tag)
{
    UINT16_t len = ntohs(packet->length);
    unsigned char *curTag;
    UINT16_t tagType, tagLen;

    if (packet->ver != 1) {
	printk(KERN_ERR "Invalid PPPoE version (%d)\n", (int) packet->ver);
	return NULL;
    }
    if (packet->type != 1) {
	printk(KERN_ERR "Invalid PPPoE type (%d)\n", (int) packet->type);
	return NULL;
    }

    /* Do some sanity checks on packet */
    if (len > ETH_DATA_LEN - 6) { /* 6-byte overhead for PPPoE header */
	printk(KERN_ERR "Invalid PPPoE packet length (%u)\n", len);
	return NULL;
    }

    /* Step through the tags */
    curTag = packet->payload;
    while(curTag - packet->payload < len) {
	/* Alignment is not guaranteed, so do this by hand... */
	tagType = (((UINT16_t) curTag[0]) << 8) +
	    (UINT16_t) curTag[1];
	tagLen = (((UINT16_t) curTag[2]) << 8) +
	    (UINT16_t) curTag[3];
	if (tagType == TAG_END_OF_LIST) {
	    return NULL;
	}
	if ((curTag - packet->payload) + tagLen + TAG_HDR_SIZE > len) {
	    printk(KERN_ERR "Invalid PPPoE tag length (%u)\n", tagLen);
	    return NULL;
	}
	if (tagType == type) {
	    memcpy(tag, curTag, tagLen + TAG_HDR_SIZE);
	    return curTag;
	}
	curTag = curTag + TAG_HDR_SIZE + tagLen;
    }
    return NULL;
}

/**********************************************************************
*%FUNCTION: addTag
*%ARGUMENTS:
* m -- a PPPoE packet
* tag -- tag to add
*%RETURNS:
* -1 if no room in packet; number of bytes added otherwise.
*%DESCRIPTION:
* Inserts a tag as the first tag in a PPPoE packet.
***********************************************************************/
int
addTag(struct sk_buff **m, PPPoETag const *tag)
{
    int len = ntohs(tag->length) + TAG_HDR_SIZE;
    PPPoEPacket *packet = MTOD(*m, PPPoEPacket *);
    int new_size, offset;
    struct sk_buff *skb;
    
    if (len + ntohs(packet->length) > MAX_PPPOE_PAYLOAD)
	return -1;
    offset = HDR_SIZE + ntohs(packet->length);
    new_size = len + offset;
    skb = skb_padto(*m, new_size);
    if (!skb)
	return -1;
    if (new_size > skb->len)
	skb_put(skb, new_size - skb->len);
    memcpy(skb->data + offset, (u8 *)tag, len);
    *m = skb;
    
    /* In case buf was realloced... */
    packet = MTOD(*m, PPPoEPacket *);
    packet->length = htons(ntohs(packet->length) + len);
    return 0;
}

/**********************************************************************
*%FUNCTION: insertBytes
*%ARGUMENTS:
* packet -- a PPPoE packet
* loc -- location at which to insert bytes of data
* bytes -- the data to insert
* len -- length of data to insert
*%RETURNS:
* -1 if no room in packet; len otherwise.
*%DESCRIPTION:
* Inserts "len" bytes of data at location "loc" in "packet", moving all
* other data up to make room.
***********************************************************************/
int
insertBytes(PPPoEPacket *packet,
	    unsigned char *loc,
	    void const *bytes,
	    int len)
{
    int toMove;
    int plen = ntohs(packet->length);
    /* Sanity checks */
    if (loc < packet->payload ||
	loc > packet->payload + plen ||
	len + plen > MAX_PPPOE_PAYLOAD) {
	return -1;
    }

    toMove = (packet->payload + plen) - loc;
    memmove(loc+len, loc, toMove);
    memcpy(loc, bytes, len);
    packet->length = htons(plen + len);
    return len;
}

/**********************************************************************
*%FUNCTION: removeBytes
*%ARGUMENTS:
* packet -- a PPPoE packet
* loc -- location at which to remove bytes of data
* len -- length of data to remove
*%RETURNS:
* -1 if there was a problem, len otherwise
*%DESCRIPTION:
* Removes "len" bytes of data from location "loc" in "packet", moving all
* other data down to close the gap
***********************************************************************/
int
removeBytes(PPPoEPacket *packet,
	    unsigned char *loc,
	    int len)
{
    int toMove;
    int plen = ntohs(packet->length);
    /* Sanity checks */
    if (len < 0 || len > plen ||
	loc < packet->payload ||
	loc + len > packet->payload + plen) {
	return -1;
    }

    toMove = ((packet->payload + plen) - loc) - len;
    memmove(loc, loc+len, toMove);
    packet->length = htons(plen - len);
    return len;
}

/**********************************************************************
*%FUNCTION: relayAddInterface
*%ARGUMENTS:
* dev -- interface name
* clientOK -- true if this interface should relay PADI, PADR packets.
* acOK -- true if this interface should relay PADO, PADS packets.
*%RETURNS:
* 0 on success, -1 on failure.
*%DESCRIPTION:
* Opens an interface; sets up discovery and session sockets.
***********************************************************************/
int relayAddInterface(struct net_device *dev, int clientOK, int acOK)
{
    pppoe_if_t *node;
    
    node = kmalloc(sizeof(pppoe_if_t), GFP_KERNEL);
    if (!node)
	return -1;

    node->next = if_head;
    if_head = node;
    node->clientOK = clientOK;
    node->acOK = acOK;
    node->dev = dev;
    return 0;
}
EXPORT_SYMBOL(relayAddInterface);

/**********************************************************************
*%FUNCTION: relayRemoveInterface 
*%ARGUMENTS:
* dev -- The device to remove.
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Removes the interface from the list of interfaces we listen on.
***********************************************************************/
void relayRemoveInterface(struct net_device *dev)
{
    pppoe_if_t **iter, *tmp;

    for (iter=&if_head; *iter && (*iter)->dev!=dev; iter=&(*iter)->next);
    if (!*iter)
	return;
    tmp = *iter;
    *iter = (*iter)->next;
    kfree(tmp);
}
EXPORT_SYMBOL(relayRemoveInterface);

/**********************************************************************
*%FUNCTION: uninitRelay
*%ARGUMENTS:
* Nothing
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Uninitializes relay hash table and session tables.
***********************************************************************/
static void uninitRelay(void)
{
   kfree(AllSessions);
   kfree(AllHashes);
}

/**********************************************************************
*%FUNCTION: initRelay
*%ARGUMENTS:
* nsess -- maximum allowable number of sessions
*%RETURNS:
* 0 on success, -1 on failure
*%DESCRIPTION:
* Initializes relay hash table and session tables.
***********************************************************************/
int
initRelay(int nsess)
{
    int i;
    NumSessions = 0;
    MaxSessions = nsess;

    AllSessions = kmalloc(MaxSessions*sizeof(PPPoESession), GFP_KERNEL);
    if (!AllSessions) {
	printk(KERN_ERR "Unable to allocate memory for PPPoE session table\n");
	return -1;
    }
    AllHashes = kmalloc(MaxSessions*2*sizeof(SessionHash), GFP_KERNEL);
    if (!AllHashes) {
	printk(KERN_ERR "Unable to allocate memory for PPPoE hash table\n");
	return -1;
    }

    /* Initialize sessions in a linked list */
    AllSessions[0].prev = NULL;
    if (MaxSessions > 1) {
	AllSessions[0].next = &AllSessions[1];
    } else {
	AllSessions[0].next = NULL;
    }
    for (i=1; i<MaxSessions-1; i++) {
	AllSessions[i].prev = &AllSessions[i-1];
	AllSessions[i].next = &AllSessions[i+1];
    }
    if (MaxSessions > 1) {
	AllSessions[MaxSessions-1].prev = &AllSessions[MaxSessions-2];
	AllSessions[MaxSessions-1].next = NULL;
    }

    FreeSessions = AllSessions;
    ActiveSessions = NULL;

    /* Initialize session numbers which we hand out */
    for (i=0; i<MaxSessions; i++) {
	AllSessions[i].sesNum = htons((UINT16_t) i+1);
    }

    /* Initialize hashes in a linked list */
    AllHashes[0].prev = NULL;
    AllHashes[0].next = &AllHashes[1];
    for (i=1; i<2*MaxSessions-1; i++) {
	AllHashes[i].prev = &AllHashes[i-1];
	AllHashes[i].next = &AllHashes[i+1];
    }
    AllHashes[2*MaxSessions-1].prev = &AllHashes[2*MaxSessions-2];
    AllHashes[2*MaxSessions-1].next = NULL;

    FreeHashes = AllHashes;
    return 0;
}

/**********************************************************************
*%FUNCTION: createSession
*%ARGUMENTS:
* ac -- Ethernet interface on access-concentrator side
* cli -- Ethernet interface on client side
* acMac -- Access concentrator's MAC address
* cliMac -- Client's MAC address
* acSess -- Access concentrator's session ID.
*%RETURNS:
* PPPoESession structure; NULL if one could not be allocated
*%DESCRIPTION:
* Initializes relay hash table and session tables.
***********************************************************************/
PPPoESession *
createSession(struct net_device *ac,
	      struct net_device *cli,
	      unsigned char const *acMac,
	      unsigned char const *cliMac,
	      UINT16_t acSes)
{
    PPPoESession *sess;
    SessionHash *acHash, *cliHash;

    if (NumSessions >= MaxSessions) {
	printk(KERN_ERR "Maximum number of sessions reached -- cannot "
	    "create new session\n");
	return NULL;
    }

    /* Grab a free session */
    sess = FreeSessions;
    FreeSessions = sess->next;
    NumSessions++;

    /* Link it to the active list */
    sess->next = ActiveSessions;
    if (sess->next) {
	sess->next->prev = sess;
    }
    ActiveSessions = sess;
    sess->prev = NULL;

    sess->epoch = jiffies;

    /* Get two hash entries */
    acHash = FreeHashes;
    cliHash = acHash->next;
    FreeHashes = cliHash->next;

    acHash->peer = cliHash;
    cliHash->peer = acHash;

    sess->acHash = acHash;
    sess->clientHash = cliHash;

    acHash->interface = ac;
    cliHash->interface = cli;

    memcpy(acHash->peerMac, acMac, ETH_ALEN);
    acHash->sesNum = acSes;
    acHash->ses = sess;

    memcpy(cliHash->peerMac, cliMac, ETH_ALEN);
    cliHash->sesNum = sess->sesNum;
    cliHash->ses = sess;

    addHash(acHash);
    addHash(cliHash);

    /* Log */
    printk(KERN_INFO
	   "Opened session: server=%02x:%02x:%02x:%02x:%02x:%02x(%s:%d), client=%02x:%02x:%02x:%02x:%02x:%02x(%s:%d)\n",
	   acHash->peerMac[0], acHash->peerMac[1],
	   acHash->peerMac[2], acHash->peerMac[3],
	   acHash->peerMac[4], acHash->peerMac[5],
	   acHash->interface->name,
	   ntohs(acHash->sesNum),
	   cliHash->peerMac[0], cliHash->peerMac[1],
	   cliHash->peerMac[2], cliHash->peerMac[3],
	   cliHash->peerMac[4], cliHash->peerMac[5],
	   cliHash->interface->name,
	   ntohs(cliHash->sesNum));

    return sess;
}

/**********************************************************************
*%FUNCTION: freeSession
*%ARGUMENTS:
* ses -- session to free
* msg -- extra message to log on syslog.
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Frees data used by a PPPoE session -- adds hashes and session back
* to the free list
***********************************************************************/
void
freeSession(PPPoESession *ses, char const *msg)
{
    printk(KERN_INFO
	   "Closed session: server=%02x:%02x:%02x:%02x:%02x:%02x(%s:%d), client=%02x:%02x:%02x:%02x:%02x:%02x(%s:%d): %s\n",
	   ses->acHash->peerMac[0], ses->acHash->peerMac[1],
	   ses->acHash->peerMac[2], ses->acHash->peerMac[3],
	   ses->acHash->peerMac[4], ses->acHash->peerMac[5],
	   ses->acHash->interface->name,
	   ntohs(ses->acHash->sesNum),
	   ses->clientHash->peerMac[0], ses->clientHash->peerMac[1],
	   ses->clientHash->peerMac[2], ses->clientHash->peerMac[3],
	   ses->clientHash->peerMac[4], ses->clientHash->peerMac[5],
	   ses->clientHash->interface->name,
	   ntohs(ses->clientHash->sesNum), msg);

    /* Unlink from active sessions */
    if (ses->prev) {
	ses->prev->next = ses->next;
    } else {
	ActiveSessions = ses->next;
    }
    if (ses->next) {
	ses->next->prev = ses->prev;
    }

    /* Link onto free list -- this is a singly-linked list, so
       we do not care about prev */
    ses->next = FreeSessions;
    FreeSessions = ses;

    unhash(ses->acHash);
    unhash(ses->clientHash);
    NumSessions--;
}

/**********************************************************************
*%FUNCTION: unhash
*%ARGUMENTS:
* sh -- session hash to free
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Frees a session hash -- takes it out of hash table and puts it on
* free list.
***********************************************************************/
void
unhash(SessionHash *sh)
{
    unsigned int b = hash(sh->peerMac, sh->sesNum) % HASHTAB_SIZE;
    if (sh->prev) {
	sh->prev->next = sh->next;
    } else {
	Buckets[b] = sh->next;
    }

    if (sh->next) {
	sh->next->prev = sh->prev;
    }

    /* Add to free list (singly-linked) */
    sh->next = FreeHashes;
    FreeHashes = sh;
}

/**********************************************************************
*%FUNCTION: addHash
*%ARGUMENTS:
* sh -- a session hash
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Adds a SessionHash to the hash table
***********************************************************************/
void
addHash(SessionHash *sh)
{
    unsigned int b = hash(sh->peerMac, sh->sesNum) % HASHTAB_SIZE;
    sh->next = Buckets[b];
    sh->prev = NULL;
    if (sh->next) {
	sh->next->prev = sh;
    }
    Buckets[b] = sh;
}

/**********************************************************************
*%FUNCTION: hash
*%ARGUMENTS:
* mac -- an Ethernet address
* sesNum -- a session number
*%RETURNS:
* A hash value combining Ethernet address with session number.
* Currently very simplistic; we may need to experiment with different
* hash values.
***********************************************************************/
unsigned int
hash(unsigned char const *mac, UINT16_t sesNum)
{
    unsigned int ans1 =
	((unsigned int) mac[0]) |
	(((unsigned int) mac[1]) << 8) |
	(((unsigned int) mac[2]) << 16) |
	(((unsigned int) mac[3]) << 24);
    unsigned int ans2 =
	((unsigned int) sesNum) |
	(((unsigned int) mac[4]) << 16) |
	(((unsigned int) mac[5]) << 24);
    return ans1 ^ ans2;
}

/**********************************************************************
*%FUNCTION: findSession
*%ARGUMENTS:
* mac -- an Ethernet address
* sesNum -- a session number
*%RETURNS:
* The session hash for peer address "mac", session number sesNum
***********************************************************************/
SessionHash *
findSession(unsigned char const *mac, UINT16_t sesNum, int ignore_peer)
{
    unsigned int b = hash(mac, sesNum) % HASHTAB_SIZE;
    SessionHash *sh = Buckets[b];
    while(sh) {
	if (sesNum == sh->sesNum && 
	    (ignore_peer || !memcmp(mac, sh->peerMac, ETH_ALEN)))
	{
	    return sh;
	}
	sh = sh->next;
    }
    return NULL;
}

void pppoe_relay_broadcast(pppoe_if_t *src_if, struct sk_buff *m)
{
    pppoe_if_t *iter;
    int used = 0;

    for (iter = if_head; iter; iter = iter->next)
    {
	struct sk_buff *mc;
	struct ethhdr *eh;

	if (iter==src_if || !iter->acOK || !(iter->dev->flags & IFF_UP))
	    continue;

	if (!iter->next)
	{
	    mc = m;
	    used = 1;
	}
	else
	{
	    mc = skb_copy(m, GFP_ATOMIC);
	    if (!mc)
	    {
		if (iter->dev->get_stats)
		    iter->dev->get_stats(iter->dev)->tx_errors++;
		continue;
	    }
	}
	eh = MTOD(mc, struct ethhdr *);
	memcpy(eh->h_source, DEV_MAC(iter->dev), ETH_ALEN);
	relay_enqueue(iter->dev, mc);
    }
    if (!used)
	kfree_skb(m);
}

/**********************************************************************
*%FUNCTION: relayGotDiscoveryPacket
*%ARGUMENTS:
* iface -- interface on which packet is recieved.
* m - packet received.
*%RETURNS:
* 0 on succsess, -1 otherwise.
*%DESCRIPTION:
* Receives and processes a discovery packet.
***********************************************************************/
int
relayGotDiscoveryPacket(pppoe_if_t *iface, struct sk_buff *m)
{
    PPPoEPacket *packet = MTOD(m, PPPoEPacket *);
    int ret = -1; 

    /* Ignore unknown code/version */
    if (packet->ver != 1 || packet->type != 1) {
	return -1;
    }

    /* Validate length */
    if (ntohs(packet->length) + HDR_SIZE > m->len) {
	printk(KERN_ERR "Bogus PPPoE length field (%u)\n",
	       (unsigned int) ntohs(packet->length));
	return -1;
    }

    switch(packet->code) {
    case CODE_PADT:
	ret = relayHandlePADT(iface, m);
	break;
    case CODE_PADI:
	ret = relayHandlePADI(iface, m);
	break;
    case CODE_PADO:
	ret = relayHandlePADO(iface, m);
	break;
    case CODE_PADR:
	ret = relayHandlePADR(iface, m);
	break;
    case CODE_PADS:
	ret = relayHandlePADS(iface, m);
	break;
    default:
	printk(KERN_ERR "Discovery packet on %s with unknown code %d\n",
	       iface->dev->name, (int) packet->code);
    }
    return ret;
}
EXPORT_SYMBOL(relayGotDiscoveryPacket);

/**********************************************************************
*%FUNCTION: relayGotSessionPacket
*%ARGUMENTS:
* iface -- interface on which packet is received.
* m - packet received.
*%RETURNS:
* 0 on succsess, -1 otherwise.
*%DESCRIPTION:
* Receives and processes a session packet.
***********************************************************************/
int
relayGotSessionPacket(pppoe_if_t *iface, struct sk_buff *m)
{
    PPPoEPacket *packet = MTOD(m ,PPPoEPacket *);
    SessionHash *sh;
    PPPoESession *ses;

    /* Ignore unknown code/version */
    if (packet->ver != 1 || packet->type != 1) {
	return -1;
    }

    /* Must be a session packet */
    if (packet->code != CODE_SESS) {
	printk(KERN_ERR "Session packet with code %d\n", (int) packet->code);
	return -1;
    }

    /* Ignore session packets whose destination address isn't ours */
    if (memcmp(packet->ethHdr.h_dest, DEV_MAC(iface->dev), ETH_ALEN))
	return -1;

    /* Validate length */
    if (ntohs(packet->length) + HDR_SIZE > m->len) {
	printk(KERN_ERR "Bogus PPPoE length field (%u)\n",
	       (unsigned int) ntohs(packet->length));
	return -1;
    }

    /* We're in business!  Find the hash */
    sh = findSession(packet->ethHdr.h_source, packet->session, 0);
    if (!sh) {
	/* Don't log this.  Someone could be running the client and the
	   relay on the same box. */
	return -1;
    }

    /* Relay it */
    ses = sh->ses;
    ses->epoch = jiffies;
    sh = sh->peer;
    packet->session = sh->sesNum;
    memcpy(packet->ethHdr.h_source, DEV_MAC(sh->interface), ETH_ALEN);
    memcpy(packet->ethHdr.h_dest, sh->peerMac, ETH_ALEN);
#if 0
    fprintf(stderr, "Relaying %02x:%02x:%02x:%02x:%02x:%02x(%s:%d) to %02x:%02x:%02x:%02x:%02x:%02x(%s:%d)\n",
	    sh->peer->peerMac[0], sh->peer->peerMac[1], sh->peer->peerMac[2],
	    sh->peer->peerMac[3], sh->peer->peerMac[4], sh->peer->peerMac[5],
	    sh->peer->interface->name, ntohs(sh->peer->sesNum),
	    sh->peerMac[0], sh->peerMac[1], sh->peerMac[2],
	    sh->peerMac[3], sh->peerMac[4], sh->peerMac[5],
	    sh->interface->name, ntohs(sh->sesNum));
#endif
    relay_enqueue(sh->interface, m);
    return 0;
}
EXPORT_SYMBOL(relayGotSessionPacket);

/**********************************************************************
*%FUNCTION: relayHandlePADT
*%ARGUMENTS:
* iface -- interface on which packet was received
* m -- the PADT packet
*%RETURNS:
* 0 on succsess, -1 otherwise.
*%DESCRIPTION:
* Receives and processes a PADT packet.
***********************************************************************/
int
relayHandlePADT(pppoe_if_t *iface,
		struct sk_buff *m)
{
    SessionHash *sh;
    PPPoESession *ses;
    PPPoEPacket *packet = MTOD(m, PPPoEPacket *);

    sh = findSession(packet->ethHdr.h_source, packet->session, 0);
    if (!sh) {
	return -1;
    }
    /* Relay the PADT to the peer */
    sh = sh->peer;
    ses = sh->ses;
    packet->session = sh->sesNum;
    memcpy(packet->ethHdr.h_source, DEV_MAC(sh->interface), ETH_ALEN);
    memcpy(packet->ethHdr.h_dest, sh->peerMac, ETH_ALEN);
    relay_enqueue(sh->interface, m);

    /* Destroy the session */
    freeSession(ses, "Received PADT");
    return 0;
}

/**********************************************************************
*%FUNCTION: relayHandlePADI
*%ARGUMENTS:
* iface -- interface on which packet was received
* m -- the PADI packet
*%RETURNS:
* 0 on success handling the packet, -1 otherwise.
*%DESCRIPTION:
* Receives and processes a PADI packet.
***********************************************************************/
int
relayHandlePADI(pppoe_if_t *iface,
		struct sk_buff *m)
{
    PPPoETag tag;
    unsigned char *loc;
    int r;
    PPPoEPacket *packet = MTOD(m, PPPoEPacket *);

    int ifIndex;

    /* Can a client legally be behind this interface? */
    if (!iface->clientOK) {
	PRINTK_DBG(
	       "PADI packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not permitted",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->dev->name);
	return -1;
    }

    /* Source address must be unicast */
    if (NOT_UNICAST(packet->ethHdr.h_source)) {
	printk(KERN_ERR
	       "PADI packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not from a unicast address\n",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->dev->name);
	return -1;
    }

    /* Destination address must be broadcast */
    if (NOT_BROADCAST(packet->ethHdr.h_dest)) {
	printk(KERN_ERR
	       "PADI packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not to a broadcast address\n",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->dev->name);
	return -1;
    }

    /* Get array index of interface */
    ifIndex = iface->dev->ifindex;

    loc = findTag(packet, TAG_RELAY_SESSION_ID, &tag);
    if (!loc) {
	u32 magic = JUNGO_RELAY_MAGIC;

	tag.type = htons(TAG_RELAY_SESSION_ID);
	tag.length = htons(MY_RELAY_TAG_LEN);
	memcpy(tag.payload, &ifIndex, sizeof(ifIndex));
	memcpy(tag.payload+sizeof(ifIndex), packet->ethHdr.h_source, ETH_ALEN);
	memcpy(tag.payload+sizeof(ifIndex) + ETH_ALEN, &magic, sizeof(magic));
	/* Add a relay tag if there's room */
	r = addTag(&m, &tag);
	if (r < 0) return -1;
    } else {
	/* We do not re-use relay-id tags.  Drop the frame.  The RFC says the
	   relay agent SHOULD return a Generic-Error tag, but this does not
	   make sense for PADI packets. */
	return -1;
    }

    /* Broadcast the PADI on all AC-capable interfaces except the interface
       on which it came */
    pppoe_relay_broadcast(iface, m);
    return 0;
}

/**********************************************************************
*%FUNCTION: relayHandlePADO
*%ARGUMENTS:
* iface -- interface on which packet was received
* m -- the PADO packet
*%RETURNS:
* 0 on success handling the packet, -1 otherwise.
*%DESCRIPTION:
* Receives and processes a PADO packet.
***********************************************************************/
int
relayHandlePADO(pppoe_if_t *iface,
		struct sk_buff *m)
{
    PPPoETag tag;
    unsigned char *loc;
    int ifIndex;
    int acIndex;
    pppoe_if_t *iter;
    PPPoEPacket *packet = MTOD(m, PPPoEPacket *);
    u32 magic;

    /* Can a server legally be behind this interface? */
    if (!iface->acOK) {
	printk(KERN_ERR
	       "PADO packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not permitted\n",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->dev->name);
	return -1;
    }

    acIndex = iface->dev->ifindex;

    /* Source address must be unicast */
    if (NOT_UNICAST(packet->ethHdr.h_source)) {
	printk(KERN_ERR
	       "PADO packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not from a unicast address\n",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->dev->name);
	return -1;
    }

    /* Destination address must be interface's MAC address */
    if (memcmp(packet->ethHdr.h_dest, DEV_MAC(iface->dev), ETH_ALEN))
	return -1;

    /* Find relay tag */
    loc = findTag(packet, TAG_RELAY_SESSION_ID, &tag);
    if (!loc) {
	PRINTK_DBG(
	       "PADO packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->dev->name);
	return -1;
    }

    /* If it's the wrong length, ignore it */
    if (ntohs(tag.length) != MY_RELAY_TAG_LEN) {
	printk(KERN_ERR
	       "PADO packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have correct length Relay-Session-Id tag\n",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->dev->name);
	return -1;
    }

    /* Extract and check Jungo magic */
    memcpy(&magic, tag.payload+MY_RELAY_TAG_LEN-sizeof(magic), sizeof(magic));
    if (magic!=JUNGO_RELAY_MAGIC)
	return -1;
    
    /* Extract interface index */
    memcpy(&ifIndex, tag.payload, sizeof(ifIndex));

    for (iter=if_head; iter && iter->dev->ifindex!=ifIndex;
	iter=iter->next);
    
    if (ifIndex < 0 || !iter ||	!iter->clientOK || iface == iter) {
	printk(KERN_ERR
	       "PADO packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s has invalid interface in Relay-Session-Id tag\n",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->dev->name);
	return -1;
    }

    /* Replace Relay-ID tag with opposite-direction tag */
    memcpy(loc+TAG_HDR_SIZE, &acIndex, sizeof(acIndex));
    memcpy(loc+TAG_HDR_SIZE+sizeof(ifIndex), packet->ethHdr.h_source, ETH_ALEN);

    /* Set destination address to MAC address in relay ID */
    memcpy(packet->ethHdr.h_dest, tag.payload + sizeof(ifIndex), ETH_ALEN);

    /* Set source address to MAC address of interface */
    memcpy(packet->ethHdr.h_source, DEV_MAC(iter->dev),	ETH_ALEN);

    /* Send the PADO to the proper client */
    relay_enqueue(iter->dev, m);
    return 0;
}

/**********************************************************************
*%FUNCTION: relayHandlePADR
*%ARGUMENTS:
* iface -- interface on which packet was received
* m -- the PADR packet
*%RETURNS:
* 0 on success handling the packet, -1 otherwise.
*%DESCRIPTION:
* Receives and processes a PADR packet.
***********************************************************************/
int
relayHandlePADR(pppoe_if_t *iface,
		struct sk_buff *m)
{
    PPPoETag tag;
    unsigned char *loc;
    int ifIndex;
    int cliIndex;
    pppoe_if_t *iter;
    PPPoEPacket *packet = MTOD(m, PPPoEPacket *);
    u32 magic;

    /* Can a client legally be behind this interface? */
    if (!iface->clientOK) {
	printk(KERN_ERR
	       "PADR packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not permitted\n",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->dev->name);
	return -1;
    }

    cliIndex = iface->dev->ifindex;

    /* Source address must be unicast */
    if (NOT_UNICAST(packet->ethHdr.h_source)) {
	printk(KERN_ERR
	       "PADR packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not from a unicast address\n",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->dev->name);
	return -1;
    }

    /* Destination address must be interface's MAC address */
    if (memcmp(packet->ethHdr.h_dest, DEV_MAC(iface->dev), ETH_ALEN))
	return -1;

    /* Find relay tag */
    loc = findTag(packet, TAG_RELAY_SESSION_ID, &tag);
    if (!loc) {
	printk(KERN_ERR
	       "PADR packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have Relay-Session-Id tag\n",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->dev->name);
	return -1;
    }

    /* If it's the wrong length, ignore it */
    if (ntohs(tag.length) != MY_RELAY_TAG_LEN) {
	printk(KERN_ERR
	       "PADR packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have correct length Relay-Session-Id tag\n",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->dev->name);
	return -1;
    }
    /* Extract and check Jungo magic */
    memcpy(&magic, tag.payload+MY_RELAY_TAG_LEN-sizeof(magic), sizeof(magic));
    if (magic!=JUNGO_RELAY_MAGIC)
	return -1;

    /* Extract interface index */
    memcpy(&ifIndex, tag.payload, sizeof(ifIndex));

    for (iter=if_head; iter && iter->dev->ifindex!=ifIndex;
	iter=iter->next);
    
    if (ifIndex < 0 || !iter ||	!iter->acOK || iface == iter) {
	printk(KERN_ERR
	       "PADR packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s has invalid interface in Relay-Session-Id tag\n",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->dev->name);
	return -1;
    }

    /* Replace Relay-ID tag with opposite-direction tag */
    memcpy(loc+TAG_HDR_SIZE, &cliIndex, sizeof(cliIndex));
    memcpy(loc+TAG_HDR_SIZE+sizeof(ifIndex), packet->ethHdr.h_source, ETH_ALEN);

    /* Set destination address to MAC address in relay ID */
    memcpy(packet->ethHdr.h_dest, tag.payload + sizeof(ifIndex), ETH_ALEN);

    /* Set source address to MAC address of interface */
    memcpy(packet->ethHdr.h_source, DEV_MAC(iter->dev),	ETH_ALEN);

    /* Send the PADR to the proper access concentrator */
    relay_enqueue(iter->dev, m);
    return 0;
}

/**********************************************************************
*%FUNCTION: relayHandlePADS
*%ARGUMENTS:
* iface -- interface on which packet was received
* m -- the PADS packet
*%RETURNS:
* 0 on success handling the packet, -1 otherwise.
*%DESCRIPTION:
* Receives and processes a PADS packet.
***********************************************************************/
int
relayHandlePADS(pppoe_if_t *iface,
		struct sk_buff *m)
{
    PPPoETag tag;
    unsigned char *loc;
    int ifIndex;
    int acIndex;
    PPPoESession *ses = NULL;
    SessionHash *sh;
    pppoe_if_t *iter;
    PPPoEPacket *packet = MTOD(m, PPPoEPacket *);
    u32 magic;

    /* Can a server legally be behind this interface? */
    if (!iface->acOK) {
	printk(KERN_ERR
	       "PADS packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not permitted\n",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->dev->name);
	return -1;
    }

    acIndex = iface->dev->ifindex;

    /* Source address must be unicast */
    if (NOT_UNICAST(packet->ethHdr.h_source)) {
	printk(KERN_ERR
	       "PADS packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not from a unicast address\n",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->dev->name);
	return -1;
    }

    /* Destination address must be interface's MAC address */
    if (memcmp(packet->ethHdr.h_dest, DEV_MAC(iface->dev), ETH_ALEN))
	return -1;

    /* Find relay tag */
    loc = findTag(packet, TAG_RELAY_SESSION_ID, &tag);
    if (!loc) {
	PRINTK_DBG(
	       "PADS packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->dev->name);
	return -1;
    }

    /* If it's the wrong length, ignore it */
    if (ntohs(tag.length) != MY_RELAY_TAG_LEN) {
	printk(KERN_ERR
	       "PADS packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have correct length Relay-Session-Id tag\n",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->dev->name);
	return -1;
    }
    /* Extract and check Jungo magic */
    memcpy(&magic, tag.payload+MY_RELAY_TAG_LEN-sizeof(magic), sizeof(magic));
    if (magic!=JUNGO_RELAY_MAGIC)
	return -1;

    /* Extract interface index */
    memcpy(&ifIndex, tag.payload, sizeof(ifIndex));
    for (iter=if_head; iter && iter->dev->ifindex!=ifIndex;
	iter=iter->next);
    if (ifIndex < 0 || !iter ||	!iter->clientOK || iface == iter) {
	printk(KERN_ERR
	       "PADS packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s has invalid interface in Relay-Session-Id tag\n",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->dev->name);
	return -1;
    }

    /* If session ID is zero, it's the AC respoding with an error.
       Just relay it; do not create a session */
    if (packet->session != htons(0)) {
	/* Check for existing session */
	sh = findSession(packet->ethHdr.h_source, packet->session, 0);
	if (sh) ses = sh->ses;

	/* If already an existing session, assume it's a duplicate PADS.  Send
	   the frame, but do not create a new session.  Is this the right
	   thing to do?  Arguably, should send an error to the client and
	   a PADT to the server, because this could happen due to a
	   server crash and reboot. */

	if (!ses) {
	    /* Create a new session */
	    ses = createSession(iface->dev, iter->dev,
				packet->ethHdr.h_source,
				loc + TAG_HDR_SIZE + sizeof(ifIndex), packet->session);
	    if (!ses) {
		/* Can't allocate session -- send error PADS to client and
		   PADT to server */
		PPPoETag hostUniq, *hu;
		if (findTag(packet, TAG_HOST_UNIQ, &hostUniq)) {
		    hu = &hostUniq;
		} else {
		    hu = NULL;
		}
		relaySendError(CODE_PADS, htons(0), iter->dev,
			       loc + TAG_HDR_SIZE + sizeof(ifIndex),
			       hu, "RP-PPPoE: Relay: Unable to allocate session");
		relaySendError(CODE_PADT, packet->session, iface->dev,
			       packet->ethHdr.h_source, NULL,
			       "RP-PPPoE: Relay: Unable to allocate session");
		return -1;
	    }
	}
	/* Replace session number */
	packet->session = ses->sesNum;
    }

    /* Remove relay-ID tag */
    removeBytes(packet, loc, MY_RELAY_TAG_LEN + TAG_HDR_SIZE);

    /* Set destination address to MAC address in relay ID */
    memcpy(packet->ethHdr.h_dest, tag.payload + sizeof(ifIndex), ETH_ALEN);

    /* Set source address to MAC address of interface */
    memcpy(packet->ethHdr.h_source, DEV_MAC(iter->dev), ETH_ALEN);

    /* Send the PADS to the proper client */
    relay_enqueue(iter->dev, m);
    return 0;
}

static int relay_send_packet(struct net_device *dev, const char *data, int len)
{
    struct sk_buff *skb = dev_alloc_skb(len + 2);
    if (!skb)
	return -1;

    skb_reserve(skb, 2);
    memcpy(skb->data, data, len);
    skb_put(skb, len);
    skb->dev = dev;
    skb->mac.raw = skb->data;
    skb->protocol = ((struct ethhdr *)skb->data)->h_proto;
    skb->nh.iph = (struct iphdr *)(skb->data + ETH_HLEN);

    return dev_queue_xmit(skb);
}

/**********************************************************************
*%FUNCTION: relaySendError
*%ARGUMENTS:
* code -- PPPoE packet code (PADS or PADT, typically)
* session -- PPPoE session number
* iface -- interface on which to send frame
* mac -- Ethernet address to which frame should be sent
* hostUniq -- if non-NULL, a hostUniq tag to add to error frame
* errMsg -- error message to insert into Generic-Error tag.
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Sends either a PADS or PADT packet with a Generic-Error tag and an
* error message.
***********************************************************************/
void
relaySendError(unsigned char code,
	       UINT16_t session,
	       struct net_device *iface,
	       unsigned char const *mac,
	       PPPoETag const *hostUniq,
	       char const *errMsg)
{
    PPPoEPacket packet;
    PPPoETag errTag;
    int size;

    memcpy(packet.ethHdr.h_source, iface->dev_addr, ETH_ALEN);
    memcpy(packet.ethHdr.h_dest, mac, ETH_ALEN);
    packet.ethHdr.h_proto = htons(ETH_P_PPP_DISC);
    packet.type = 1;
    packet.ver = 1;
    packet.code = code;
    packet.session = session;
    packet.length = htons(0);
    if (hostUniq) {
	if (insertBytes(&packet, (char *)&packet.payload, hostUniq,
	    ntohs(hostUniq->length) + TAG_HDR_SIZE) < 0)
	{
	    return;
	}
    }
    errTag.type = htons(TAG_GENERIC_ERROR);
    errTag.length = htons(strlen(errMsg));
    strcpy(errTag.payload, errMsg);
    if (insertBytes(&packet, (char *)&packet.payload, &errTag,
	ntohs(errTag.length) + TAG_HDR_SIZE) < 0)
    {
	return;
    }
    size = ntohs(packet.length) + HDR_SIZE;
    relay_send_packet(iface, (char *)&packet, size);
}

/**********************************************************************
*%FUNCTION: cleanSessions
*%ARGUMENTS:
* timeout -- The timeout to check against when expiring sessions.
*%RETURNS:
* The timeout to the next possible expiry (in cpu ticks).
*%DESCRIPTION:
* Goes through active sessions and cleans sessions idle for longer
* than IdleTimeout seconds.
***********************************************************************/
unsigned long cleanSessions(unsigned long timeout)
{
    PPPoESession *cur, *next;
    unsigned long min = ULONG_MAX;
    cur = ActiveSessions;
    while(cur) {
	next = cur->next;
	if (((int)(cur->epoch + timeout - jiffies)) <= 0)
	{
	    /* Send PADT to each peer */
	    relaySendError(CODE_PADT, cur->acHash->sesNum,
			   cur->acHash->interface,
			   cur->acHash->peerMac, NULL,
			   "RP-PPPoE: Relay: Session exceeded idle timeout");
	    relaySendError(CODE_PADT, cur->clientHash->sesNum,
			   cur->clientHash->interface,
			   cur->clientHash->peerMac, NULL,
			   "RP-PPPoE: Relay: Session exceeded idle timeout");
	    freeSession(cur, "Idle Timeout");
	}
	else if (cur->epoch < min)
	    min = cur->epoch;
	cur = next;
    }
    return min==ULONG_MAX ? timeout : timeout - (jiffies - min);
}
EXPORT_SYMBOL(cleanSessions);

int pppoe_relay_session_exists(struct sk_buff *m)
{
    PPPoEPacket *packet = MTOD(m ,PPPoEPacket *);
    return findSession(packet->ethHdr.h_source, packet->session, 1) ? 1 : 0;
}
EXPORT_SYMBOL(pppoe_relay_session_exists);

pppoe_if_t *pppoe_relay_dev_get(struct net_device *dev)
{
    pppoe_if_t *iter;

    for (iter = if_head; iter && iter->dev != dev; iter = iter->next);
    return iter;
}
EXPORT_SYMBOL(pppoe_relay_dev_get);

void relayInterfacesClear(void)
{
    while (if_head)
    {
	pppoe_if_t *tmp = if_head;
	
	if_head = if_head->next;
	kfree(tmp);
    }
}
EXPORT_SYMBOL(relayInterfacesClear);

/* Called from openrg_init() if linked statically or from init_module() */
int pppoe_relay_init(void)
{
    if (initRelay(DEFAULT_SESSIONS))
	return -ENOMEM;
    return 0;
}

void pppoe_relay_uninit(void)
{
    uninitRelay();
}

#ifdef MODULE
int init_module(void)
{
    return pppoe_relay_init();
}

void cleanup_module(void)
{
    pppoe_relay_uninit();
}
#endif

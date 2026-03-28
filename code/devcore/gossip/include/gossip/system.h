#include <stddef.h>
#include <stdint.h>
#include <base/prot_array.h>
#include <packproto/proto.h>
#include <multithr/time.h>
#include <lownet/core.h>
#include <crypto/system.h>

#ifndef GOSSIP_SYSTEM_H
#define GOSSIP_DEAD_DT (60 * 60) // one hour

typedef struct __attribute__((packed)) {
    char    pubkey[CRYPTO_PUBKEY_BYTES];
    naddr_t available_state_server;
} gossip_metadata;

typedef struct __attribute__((packed)) {
    uint32_t uid;       // UID
    naddr_t  addr;
    uint32_t version;   // how many retranslates
    uint64_t timestamp; // when was created

    gossip_metadata md;
} gossip_entry;

// no daemon, works only as data storage
typedef struct {
    prot_array gossips;
    uint32_t   last_gossiped;
    uint32_t   self_uid;
} gossip_system;

int gossip_system_init(gossip_system *g, uint32_t self_uid);
int gossip_system_end (gossip_system *g);

// remove all timeouted entries
int gossip_cleanup    (gossip_system *g);

// just register new entry (copy)
int gossip_new_entry(gossip_system *g, const gossip_entry *entry);

// * get N randomly chosen entries (copies)
// * entries MUST be NULL and will be malloced after function
int gossip_random_entries(gossip_system *g, gossip_entry **entries, size_t *n);

// simultaneously extract data from packet (N entries) and add them to the system
int gossip_from_packet(gossip_system *g, protopack *pack_in);

// * pack entries to data (N given entries)
int gossip_to_data(gossip_system *g, gossip_entry **entries, size_t n, uint8_t **out_data, size_t *out_sz);

// data: [4 bytes: N of entries][entry.dsize + memcpy(entry) (for each of entries dsize is unique) for N ]

int gossip_entry_copy(gossip_entry *dest, const gossip_entry *src);

gossip_entry gossip_create_entry(
    uint32_t uid,       // UID
    naddr_t *addr,
    gossip_metadata md,
    bool     convert_hton
);

#endif
#define GOSSIP_SYSTEM_H

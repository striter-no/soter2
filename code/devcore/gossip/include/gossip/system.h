#include <stddef.h>
#include <stdint.h>
#include <base/prot_array.h>
#include <packproto/proto.h>

#ifndef GOSSIP_SYSTEM_H
#define GOSSIP_DEAD_DT (60 * 60) // one hour

#pragma pack(push, 1)
typedef struct {
    uint32_t uid;       // UID
    uint32_t ip;        // used IP
    uint16_t port;      // opened/used port
    uint32_t version;   // how many retranslates
    uint32_t timestamp; // when was created

    uint32_t mtd_size;
    char     metadata[];  // can be public key, signature etc.
} gossip_entry;
#pragma pack(pop)

// no daemon, works only as data storage
typedef struct {
    prot_array gossips;
} gossip_system;

int gossip_system_init(gossip_system *g);
int gossip_system_end (gossip_system *g);

// remove all timeouted entries
int gossip_cleanup    (gossip_system *g);

// just register new entry (copy)
int gossip_new_entry(gossip_system *g, gossip_entry *entry);

// * get N randomly chosen entries (copies)
// * entries MUST be pre-allocated
int gossip_random_entries(gossip_system *g, gossip_entry **entries, size_t n);

// simultaneously extract data from packet (N entries) and add them to the system
int gossip_from_packet(gossip_system *g, protopack *pack_in);

// * pack entries to data (N given entries)
int gossip_to_data(gossip_system *g, gossip_entry **entries, size_t n, uint8_t **out_data);

// data: [4 bytes: N of entries][entry.dsize + memcpy(entry) (for each of entries dsize is unique) for N ]

int gossip_entry_copy(gossip_entry **dest, gossip_entry *src);

#endif
#define GOSSIP_SYSTEM_H
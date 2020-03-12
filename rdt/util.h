#ifndef _RDT_UTIL_H_
#define _RDT_UTIL_H_

#include "rdt_struct.h"

#ifndef GBN
#define GBN 0
#endif

#define DEBUG 0

typedef unsigned int seq_t;
#define SEQ_SIZE sizeof(seq_t)
typedef unsigned char pls_t;
#define PLS_SIZE sizeof(pls_t)
typedef unsigned short checksum_t;
#define CHECKSUM_SIZE sizeof(checksum_t)

#define REC_WINDOW_SIZE 4
#define WINDOW_SIZE (2 * REC_WINDOW_SIZE)
#define MAX_SEQ (3 * WINDOW_SIZE)

// a <= b < c, circularly
bool between(seq_t a, seq_t b, seq_t c);
// a < b, circularly, in a window of size ws
bool less_than(seq_t a, seq_t b, int ws);
void inc_circularly(seq_t &seq);

struct packet_header {
    char checksum[CHECKSUM_SIZE];
    char payload_size[PLS_SIZE];
    char sequence_num[SEQ_SIZE];
};

#define HEADER_SIZE sizeof(packet_header)
#define MAX_PLS RDT_PKTSIZE - HEADER_SIZE

pls_t *ref_payload_size(packet *pkt);
seq_t *ref_seq(packet *pkt);
checksum_t *ref_checksum(packet *pkt);

pls_t get_payload_size(const packet *pkt);
seq_t get_seq(const packet *pkt);
checksum_t get_checksum(const packet *pkt);

checksum_t gen_checksum(char *buf, int nword);
bool verify_checksum(char *buf, int nword, checksum_t checksum);

checksum_t sum(packet *pkt);
bool sanity_check(packet *pkt);

#define ACK_PLS 3
void nak(packet *pkt);
bool is_nak(packet *pkt);

double GetSimulationTime();
void debug_printf(const char *format, ...);

#endif
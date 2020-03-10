#ifndef _RDT_UTIL_H_
#define _RDT_UTIL_H_

#include "rdt_struct.h"

typedef unsigned int seq_t;
#define SEQ_SIZE sizeof(seq_t)
typedef unsigned char pls_t;
#define PLS_SIZE sizeof(pls_t)
typedef unsigned short checksum_t;
#define CHECKSUM_SIZE sizeof(checksum_t)

#define MAX_SEQ 16

// a <= b < c, circularly
bool between(seq_t a, seq_t b, seq_t c);
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

checksum_t gen_checksum(char *buf, int nword);
bool verify_checksum(char *buf, int nword, checksum_t checksum);

checksum_t sum(packet *pkt);
bool sanity_check(packet *pkt);

#define DEBUG 0

double GetSimulationTime();
void debug_printf(const char *format, ...);

#endif
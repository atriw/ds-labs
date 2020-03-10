#include "util.h"
#include <cstdarg>
#include <stdio.h>

// a <= b < c, circularly
bool between(seq_t a, seq_t b, seq_t c) {
    return (a <= b && b < c) || (c < a && a <= b) || (b < c && c < a);
}

void inc_circularly(seq_t &seq) {
    seq++;
    if (seq >= MAX_SEQ) {
        seq = 0;
    }
}

pls_t *ref_payload_size(packet *pkt) {
    return (pls_t *)((packet_header *)pkt)->payload_size;
}

seq_t *ref_seq(packet *pkt) {
    return (seq_t *)((packet_header *)pkt)->sequence_num;
}

checksum_t *ref_checksum(packet *pkt) {
    return (checksum_t *)((packet_header *)pkt)->checksum;
}

checksum_t gen_checksum(char *buf, int nword)
{
    unsigned long sum;
 
    for(sum = 0; nword > 0; nword--)
        sum += *buf++;             
 
    sum  = (sum>>16) + (sum&0xffff);
    sum += (sum>>16);
 
    return ~sum;
}

bool verify_checksum(char *buf, int nword, checksum_t checksum)
{
    unsigned long sum;
 
    for(sum = 0; nword > 0; nword--)
        sum += *buf++;             
 
    sum  = (sum>>16) + (sum&0xffff);
    sum += (sum>>16);

    return (((sum + checksum) & 0xff) == 0xff);
}

checksum_t sum(packet *pkt) {
    return gen_checksum(pkt->data+CHECKSUM_SIZE, *(ref_payload_size(pkt))+HEADER_SIZE-CHECKSUM_SIZE);
}

bool sanity_check(packet *pkt) {
    pls_t payload_size = *(ref_payload_size(pkt));
    checksum_t checksum = *(ref_checksum(pkt));
    seq_t seq = *(ref_seq(pkt));
    return payload_size <= MAX_PLS && seq < MAX_SEQ && verify_checksum(pkt->data+CHECKSUM_SIZE, payload_size+HEADER_SIZE-CHECKSUM_SIZE, checksum);
}

void debug_printf(const char *format, ...) {
    if (DEBUG) {
        va_list args;
        va_start(args, format);
        fprintf(stdout, "At %.2fs: ", GetSimulationTime());
        vfprintf(stdout, format, args);
        fprintf(stdout, "\n");
        va_end(args);
    }
}
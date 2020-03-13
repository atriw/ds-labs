#include "util.h"
#include <cstdarg>
#include <stdio.h>

// a <= b < c, circularly
bool between(seq_t a, seq_t b, seq_t c) {
    return (a <= b && b < c) || (c < a && a <= b) || (b < c && c < a);
}

bool less_than(seq_t expected, seq_t in, int ws) {
    return expected != in && between(expected, in, (expected+ws)%MAX_SEQ);
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

pls_t get_payload_size(const packet *pkt) {
    return *(ref_payload_size(const_cast<packet *>(pkt)));
}

seq_t get_seq(const packet *pkt) {
    return *(ref_seq(const_cast<packet *>(pkt)));
}

checksum_t get_checksum(const packet *pkt) {
    return *(ref_checksum(const_cast<packet *>(pkt)));
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
    return gen_checksum(pkt->data+CHECKSUM_SIZE, get_payload_size(pkt)+HEADER_SIZE-CHECKSUM_SIZE);
}

bool sanity_check(packet *pkt) {
    pls_t payload_size = get_payload_size(pkt);
    checksum_t checksum = get_checksum(pkt);
    seq_t seq = get_seq(pkt);
    return payload_size <= MAX_PLS && seq < MAX_SEQ && verify_checksum(pkt->data+CHECKSUM_SIZE, payload_size+HEADER_SIZE-CHECKSUM_SIZE, checksum);
}

void nak(packet *pkt) {
    pkt->data[HEADER_SIZE] = 'N';
    pkt->data[HEADER_SIZE+1] = 'A';
    pkt->data[HEADER_SIZE+2] = 'K';
}

bool is_nak(packet *pkt) {
    return pkt->data[HEADER_SIZE] == 'N' && pkt->data[HEADER_SIZE+1] == 'A' && pkt->data[HEADER_SIZE+2] == 'K';
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
/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
 * NOTE: This implementation assumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 *       |<-  1 byte  ->|<-      4 byte     ->|<-  2 byte  ->|<-        the rest        ->|
 *       | payload size |<- sequence number ->|<- checksum ->|<-        payload         ->|
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rdt_struct.h"
#include "rdt_receiver.h"

typedef unsigned int seq_nr;
#define MAX_SEQ 4294967295
#define PAYLOAD_HEADER_SIZE sizeof(char)
#define SEQ_SIZE sizeof(unsigned int)
#define CHECKSUM_SIZE sizeof(unsigned short)
#define get_seq(pkt) (*((seq_nr *)((pkt)->data + PAYLOAD_HEADER_SIZE)))
#define HEADER_SIZE (PAYLOAD_HEADER_SIZE + SEQ_SIZE + CHECKSUM_SIZE)

struct pkt_node
{
    packet pkt;
    pkt_node *next = NULL;
};

static seq_nr seq = 0;

static void inc_seq()
{
    if (seq < MAX_SEQ) {
        seq++;
    } else {
        seq = 0;
    }
}

static int count(pkt_node *list)
{
    if (list == NULL) return 0;
    return 1 + count(list->next);
}

static unsigned short gen_checksum(char *buf, int nword)
{
    unsigned long sum;
 
    for(sum = 0; nword > 0; nword--)
        sum += *buf++;             
 
    sum  = (sum>>16) + (sum&0xffff);
    sum += (sum>>16);

    return ~sum;
}

static bool verify_checksum(char *buf, int nword, unsigned short checksum)
{
    unsigned long sum;
 
    for(sum = 0; nword > 0; nword--)
        sum += *buf++;             
 
    sum  = (sum>>16) + (sum&0xffff);
    sum += (sum>>16);

    return (((sum + checksum) & 0xff) == 0xff);
}

static pkt_node *pkt_list = NULL;
static pkt_node *pkt_tail = NULL;

static pkt_node *pkt_new(packet *pkt)
{
    pkt_node *node = (pkt_node *)malloc(sizeof(*node));
    memcpy(&node->pkt, pkt, sizeof(packet));
    node->next = NULL;
    return node;
}

static void suspend(packet *pkt)
{
    pkt_node *node = pkt_new(pkt);
    if (pkt_list == NULL) {
        pkt_list = pkt_tail = node;
    } else {
        for (pkt_node *p=pkt_list, *prev = NULL; p; prev = p, p=p->next) {
            seq_nr p_seq = *((seq_nr *)(p->pkt.data + 1));
            seq_nr pkt_seq = *((seq_nr *)(pkt->data + 1));
            if (pkt_seq == p_seq) {
                free(node);
                return;
            } else if (pkt_seq < p_seq) {
                node->next = p;
                if (prev) {
                    prev->next = node;
                } else {
                    pkt_list = node;
                }
                return;
            }
        }
        pkt_tail->next = node;
        pkt_tail = pkt_tail->next;
    }
}

static void consume(seq_nr k);

static void send_ack(seq_nr k)
{
    packet ack;

    int maxpayload_size = RDT_PKTSIZE - HEADER_SIZE;

    ack.data[0] = maxpayload_size;
    *((seq_nr *)(ack.data + 1)) = k;
    memset(ack.data + HEADER_SIZE, 1, maxpayload_size);
    unsigned short cs = gen_checksum(ack.data+HEADER_SIZE, maxpayload_size);
    *((unsigned short *)(ack.data + PAYLOAD_HEADER_SIZE + SEQ_SIZE)) = cs;
    Receiver_ToLowerLayer(&ack);
}

static void confirm(packet *pkt)
{
    /* 1-byte header indicating the size of the payload */
    int header_size = HEADER_SIZE;

    /* construct a message and deliver to the upper layer */
    struct message *msg = (struct message*) malloc(sizeof(struct message));
    ASSERT(msg!=NULL);

    msg->size = pkt->data[0];

    /* sanity check in case the packet is corrupted */
    if (msg->size<0) msg->size=0;
    if (msg->size>RDT_PKTSIZE-header_size) msg->size=RDT_PKTSIZE-HEADER_SIZE;

    msg->data = (char*) malloc(msg->size);
    ASSERT(msg->data!=NULL);
    memcpy(msg->data, pkt->data+HEADER_SIZE, msg->size);
    Receiver_ToUpperLayer(msg);

    /* don't forget to free the space */
    if (msg->data!=NULL) free(msg->data);
    if (msg!=NULL) free(msg);

    send_ack(seq);
    inc_seq();
    consume(seq);
}

static void consume(seq_nr k)
{
    if (pkt_list && (*((seq_nr *)(pkt_list->pkt.data + 1))) == k) {
        pkt_node *p = pkt_list;
        pkt_list = pkt_list->next;
        confirm(&p->pkt);
        free(p);
    }
}


/* receiver initialization, called once at the very beginning */
void Receiver_Init()
{
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some 
   memory you allocated in Receiver_init(). */
void Receiver_Final()
{
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a packet is passed from the lower layer at the 
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt)
{
    int payload_size = pkt->data[0];
    seq_nr actual_seq = *((seq_nr *)(pkt->data + PAYLOAD_HEADER_SIZE));

    unsigned short checksum = *((unsigned short *)(pkt->data + PAYLOAD_HEADER_SIZE + SEQ_SIZE));
    if (!verify_checksum(pkt->data+HEADER_SIZE, payload_size, checksum))
        return;

    if (actual_seq > seq) {
        suspend(pkt);
    } else if (actual_seq == seq) {
        confirm(pkt);
    } else {
        send_ack(actual_seq);
    }
}

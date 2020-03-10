/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
 * NOTE: This implementation assumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 *       |<-  2 byte  ->|<-  1 byte  ->|<-      4 byte     ->|<-        the rest        ->|
 *       |<- checksum ->| payload size |<- sequence number ->|<-        payload         ->|
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rdt_struct.h"
#include "rdt_receiver.h"

#include "util.h"

static seq_t expected_seq = 0;

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

void acknowledge(seq_t seq) {
    packet ack;
    *(ref_payload_size(&ack)) = MAX_PLS;
    *(ref_seq(&ack)) = seq;
    *(ref_checksum(&ack)) = sum(&ack);

    Receiver_ToLowerLayer(&ack);
}

/* event handler, called when a packet is passed from the lower layer at the 
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt)
{
    /* 1-byte header indicating the size of the payload */
    // int header_size = sizeof(packet_header);

    pls_t payload_size = *(ref_payload_size(pkt));
    checksum_t checksum = *(ref_checksum(pkt));
    seq_t seq = *(ref_seq(pkt));
    
    debug_printf("receive a packet, payload_size %d, checksum %d, seq %d, expected %d", payload_size, checksum, seq, expected_seq);

    if (!sanity_check(pkt)) {
        debug_printf("drop");
        return;
    }

    if (seq != expected_seq) {
        return;
    }

    acknowledge(expected_seq);
    inc_circularly(expected_seq);

    /* construct a message and deliver to the upper layer */
    struct message *msg = (struct message*) malloc(sizeof(struct message));
    ASSERT(msg!=NULL);

    msg->size = payload_size;

    /* sanity check in case the packet is corrupted */
    // if (msg->size<0) msg->size=0;
    // if (msg->size>RDT_PKTSIZE-HEADER_SIZE) msg->size=RDT_PKTSIZE-HEADER_SIZE;

    msg->data = (char*) malloc(msg->size);
    ASSERT(msg->data!=NULL);
    memcpy(msg->data, pkt->data+HEADER_SIZE, msg->size);
    Receiver_ToUpperLayer(msg);

    /* don't forget to free the space */
    if (msg->data!=NULL) free(msg->data);
    if (msg!=NULL) free(msg);
}

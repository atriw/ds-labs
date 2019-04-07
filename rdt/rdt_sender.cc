/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
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
#include <queue>

#include "rdt_struct.h"
#include "rdt_sender.h"

#define WINDOW_SIZE 10
#define MAX_SEQ 4294967295
#define DEFAULT_TIMEOUT 0.3
#define between(a, b, c) (((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((b < c) && (c < a)))
#define PAYLOAD_HEADER_SIZE sizeof(char)
#define SEQ_SIZE sizeof(unsigned int)
#define CHECKSUM_SIZE sizeof(unsigned short)
#define get_seq(pkt) (*((seq_nr *)((pkt)->data + PAYLOAD_HEADER_SIZE)))
#define HEADER_SIZE (PAYLOAD_HEADER_SIZE + SEQ_SIZE + CHECKSUM_SIZE)

typedef unsigned int seq_nr;

//----------------------------------------------------------------
// DEBUG
seq_nr getseq(packet *pkt)
{
    return get_seq(pkt);
}

//----------------------------------------------------------------
// vitural timer implemented on one physical timer
struct vtimer
{
    double timeout;
    double start;
    seq_nr num;
    vtimer *next = NULL;
};

static vtimer *timer_list = NULL;
static vtimer *timer_tail = NULL;

static void destroy(vtimer *p)
{
    if (p == NULL) return;
    destroy(p->next);
    free(p);
}

static void append(vtimer *timer) 
{
    if (timer_list == NULL) {
        timer_list = timer_tail = timer;
    } else {
        timer_tail->next = timer;
        timer_tail = timer_tail->next;
    }
}

static void pop()
{
    if (timer_list) {
        vtimer *p = timer_list;
        if (timer_tail == timer_list) 
            timer_list = timer_tail = NULL;
        else timer_list = timer_list->next;
        free(p);
    }
}

static vtimer *new_timer(double timeout, seq_nr k)
{
    vtimer *p = (vtimer *)malloc(sizeof(*p));
    p->timeout = timeout;
    p->start = GetSimulationTime();
    p->num = k;
    p->next = NULL;
    return p;
}

static void register_timer(double timeout, seq_nr k)
{
    vtimer *p = new_timer(timeout, k);
    append(p);
    if (!Sender_isTimerSet()) 
        Sender_StartTimer(timeout);
}

static void start_timer()
{
    ASSERT(timer_list != NULL);
    double timeout = timer_list->timeout - (GetSimulationTime() - timer_list->start);
    if (timeout > 0) {
        Sender_StartTimer(timeout);
    } else {
        Sender_Timeout();
    }
}

static void stop_timer(seq_nr k)
{
    for (vtimer *p=timer_list, *prev=NULL; p; prev = p, p=p->next) {
        if (p->num == k) {
            if (p == timer_list) {
                Sender_StopTimer();
                pop();
                if (timer_list) start_timer();
            } else {
                prev->next = p->next;
                free(p);
            }
            return;
        }
    }
}

//----------------------------------------------------------------

static packet window[WINDOW_SIZE + 1];
static seq_nr nbuffered = 0;
static seq_nr seq = 0;
static seq_nr ack = 0;

static void inc_seq()
{
    if (seq < MAX_SEQ) {
        seq++;
    } else {
        seq = 0;
    }
}

static void inc_ack()
{
    if (ack < MAX_SEQ) {
        ack++;
    } else {
        ack = 0;
    }
}
//----------------------------------------------------------------
// sender buffer
struct buffer
{
    packet pkt;
    buffer *next = NULL;
};

static buffer *buffer_list = NULL;
static buffer *buffer_tail = NULL;

static buffer *buffer_new(packet *pkt)
{
    buffer *p = (buffer *)malloc(sizeof(*p));
    memcpy(&p->pkt, pkt, sizeof(packet));
    p->next = NULL;
    return p;
}

static void buffer_push(packet *pkt)
{
    buffer *p = buffer_new(pkt);
    if (buffer_list == NULL) {
        buffer_list = buffer_tail = p;
    } else {
        buffer_tail->next = p;
        buffer_tail = buffer_tail->next;
    }
}

static buffer *buffer_pop()
{
    if (buffer_list) {
        buffer *p = buffer_list;
        if (buffer_tail == buffer_list) 
            buffer_list = buffer_tail = NULL;
        else 
            buffer_list = buffer_list->next;
        return p;
    }
    return NULL;
}

static unsigned int buffer_count(buffer *list)
{
    if (list == NULL) return 0;
    return 1 + buffer_count(list->next);
}

static bool buffer_empty()
{
    return buffer_list == NULL;
}
//----------------------------------------------------------------
/* sender initialization, called once at the very beginning */
void Sender_Init()
{
    nbuffered = 0;
    seq = 0;
    ack = 0;
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some 
   memory you allocated in Sender_init(). */
void Sender_Final()
{
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
}
//----------------------------------------------------------------
static void send(packet *pkt)
{
    seq_nr seq_expected = get_seq(pkt);
    memcpy(window+(seq_expected % WINDOW_SIZE), pkt, sizeof(packet));
    nbuffered = nbuffered + 1;
    
	/* send it out through the lower layer */
	Sender_ToLowerLayer(window+(seq_expected % WINDOW_SIZE));
    register_timer(DEFAULT_TIMEOUT, seq_expected);
}

static void suspend(packet *pkt)
{
    buffer_push(pkt);
}
//----------------------------------------------------------------
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
//----------------------------------------------------------------
/* event handler, called when a message is passed from the upper layer at the 
   sender */
void Sender_FromUpperLayer(struct message *msg)
{
    /* 1-byte header indicating the size of the payload */
    // int header_size = PAYLOAD_HEADER_SIZE + SEQ_SIZE + CHECKSUM_SIZE;
    

    /* maximum payload size */
    int maxpayload_size = RDT_PKTSIZE - HEADER_SIZE;

    /* split the message if it is too big */

    /* reuse the same packet data structure */
    packet pkt;

    /* the cursor always points to the first unsent byte in the message */
    int cursor = 0;

    while (msg->size-cursor > maxpayload_size) {
	/* fill in the packet */
	pkt.data[0] = maxpayload_size;
    *((seq_nr *)(pkt.data + PAYLOAD_HEADER_SIZE)) = seq;
	memcpy(pkt.data+HEADER_SIZE, msg->data+cursor, maxpayload_size);
    // set checksum
    unsigned short cs = gen_checksum(pkt.data+HEADER_SIZE, maxpayload_size);
    *((unsigned short *)(pkt.data + PAYLOAD_HEADER_SIZE + SEQ_SIZE)) = cs;

    if (nbuffered < WINDOW_SIZE) {
        send(&pkt);
    } else { // window is full
        suspend(&pkt);
    }
    inc_seq();

	/* move the cursor */
	cursor += maxpayload_size;
    }

    /* send out the last packet */
    if (msg->size > cursor) {
	/* fill in the packet */
	pkt.data[0] = msg->size-cursor;
    *((seq_nr *)(pkt.data + 1)) = seq;
	memcpy(pkt.data+HEADER_SIZE, msg->data+cursor, pkt.data[0]);

    unsigned short cs = gen_checksum(pkt.data+HEADER_SIZE, msg->size-cursor);
    *((unsigned short *)(pkt.data + PAYLOAD_HEADER_SIZE + SEQ_SIZE)) = cs;

    if (nbuffered < WINDOW_SIZE) {
        send(&pkt);
    } else {
        suspend(&pkt);
    }
    inc_seq();
    }
}

/* event handler, called when a packet is passed from the lower layer at the 
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
    int payload_size = pkt->data[0];
    unsigned short checksum = *((unsigned short *)(pkt->data + PAYLOAD_HEADER_SIZE + SEQ_SIZE));
    if (!verify_checksum(pkt->data+HEADER_SIZE, payload_size, checksum))
        return;

    seq_nr actual_ack = *((seq_nr *)(pkt->data + 1));
    while(between(ack, actual_ack, seq)) {
        nbuffered = nbuffered - 1;
        while(nbuffered < WINDOW_SIZE && !buffer_empty()){
            buffer *p = buffer_pop();
            send(&(p->pkt));
            free(p);
        }
        stop_timer(ack);
        inc_ack();
    }
}

/* event handler, called when the timer expires */
void Sender_Timeout()
{
    seq_nr a_seq = ack;
    if (timer_list) destroy(timer_list);
    timer_list = timer_tail = NULL;
    for (seq_nr i = 0; i < nbuffered; i++) {
        Sender_ToLowerLayer(window + (a_seq % WINDOW_SIZE));
        register_timer(DEFAULT_TIMEOUT, a_seq);
        a_seq++;
    }
}


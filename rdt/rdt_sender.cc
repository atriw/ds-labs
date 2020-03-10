/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
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
#include "rdt_sender.h"

#include <list>
#include <queue>

#include "util.h"

using namespace std;

template<class K>
class timerInfo {
public:
    timerInfo(K key, double timeout, double start)
    : key(key), timeout(timeout), start(start) {}
    K key;

    // in seconds
    double timeout;
    double start;
};

template<class K>
class Vtimer
{
private:
    // assume all timeouts are same, maybe priority_queue?
    list<timerInfo<K>> chain = list<timerInfo<K>>();
public:
    void Start(K key, double timeout) {
        chain.emplace_back(key, timeout, GetSimulationTime());
        if (!Sender_isTimerSet()) {
            Sender_StartTimer(timeout);
        }
    }

    void Stop(K key) {
        if (!chain.empty() && chain.front().key == key) {
            Sender_StopTimer();
        }
        chain.remove_if([&](timerInfo<K> &ti) { return ti.key == key; });
        if (!Sender_isTimerSet() && !chain.empty()) {
            timerInfo<K> first = chain.front();
            Sender_StartTimer(first.timeout - (GetSimulationTime() - first.start));
        }
    }

    void Timeout() {
        chain.clear();
    }
};

Vtimer<seq_t> timer = Vtimer<seq_t>();

#define WINDOW_SIZE 8
static packet window[WINDOW_SIZE];
static int in_window = 0;

#define DEFAULT_TIMEOUT 0.3

static queue<packet> buffer = queue<packet>();

// next to send
static seq_t next_seq = 0;

// extra sequence num in order not to block upper layer
static seq_t pack_seq = 0;

static seq_t expected_ack = 0;

// acknowledge that expected_ack has been received
void acknowledge() {
    in_window--;
    timer.Stop(expected_ack);
    inc_circularly(expected_ack);
}

/* sender initialization, called once at the very beginning */
void Sender_Init()
{
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

void pack(packet *pkt, seq_t seq, char *payload, int payload_size) {
    memcpy(pkt->data+HEADER_SIZE, payload, payload_size);
    *(ref_payload_size(pkt)) = (pls_t)payload_size;
    *(ref_seq(pkt)) = seq;
    *(ref_checksum(pkt)) = sum(pkt);
}

void send(packet *pkt) {
    if (in_window < WINDOW_SIZE) {
        seq_t seq = *(ref_seq(pkt));
        window[seq % WINDOW_SIZE] = *pkt;
        in_window++;
        debug_printf("send seq %d", seq);
        inc_circularly(next_seq);
        Sender_ToLowerLayer(pkt);
        timer.Start(seq, DEFAULT_TIMEOUT);
    } else {
        // window is full
        buffer.push(*pkt);
    }
}

/* event handler, called when a message is passed from the upper layer at the 
   sender */
void Sender_FromUpperLayer(struct message *msg)
{
    debug_printf("get a message, len %d", msg->size);
    /* 1-byte header indicating the size of the payload */
    // int header_size = sizeof(packet_header);

    /* maximum payload size */
    // int maxpayload_size = RDT_PKTSIZE - header_size;

    /* split the message if it is too big */

    /* reuse the same packet data structure */
    packet pkt;

    /* the cursor always points to the first unsent byte in the message */
    int cursor = 0;

    while (msg->size-cursor > (int)(MAX_PLS)) {
	/* fill in the packet */
	pack(&pkt, pack_seq, msg->data+cursor, MAX_PLS);
    inc_circularly(pack_seq);

	/* send it out through the lower layer */
    send(&pkt);

	/* move the cursor */
	cursor += MAX_PLS;
    }

    /* send out the last packet */
    if (msg->size > cursor) {
	/* fill in the packet */
	pack(&pkt, pack_seq, msg->data+cursor, msg->size-cursor);
    inc_circularly(pack_seq);

	/* send it out through the lower layer */
    send(&pkt);
    }
}

/* event handler, called when a packet is passed from the lower layer at the 
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
    if (!sanity_check(pkt)) {
        return;
    }

    seq_t actual_ack = *(ref_seq(pkt));
    debug_printf("receive ack %d, expected %d, next_seq %d", actual_ack, expected_ack, next_seq);
    while (between(expected_ack, actual_ack, next_seq)) {
        acknowledge();
        while (in_window < WINDOW_SIZE && !buffer.empty()) {
            packet p = buffer.front();
            buffer.pop();
            send(&p);
        }
    }

}

/* event handler, called when the timer expires */
void Sender_Timeout()
{
    // cancel all timers
    timer.Timeout();
    // resend all non-ack packets in window
    int count = in_window;
    in_window = 0;

    seq_t seq = expected_ack;
    next_seq = seq;
    for (int i = 0; i < count; ++seq, ++i) {
        send(window+(seq % WINDOW_SIZE));
    }
}

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

#define DEFAULT_TIMEOUT 0.3

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
            double timeout = first.timeout - (GetSimulationTime() - first.start);
            if (timeout > 0) {
                Sender_StartTimer(timeout);
            } else {
                Sender_Timeout();
            }
        }
    }

    void Timeout() {
        chain.clear();
    }
};

template<int S>
class Window
{
private:
    packet window[S];
    int in_window;
    seq_t expected_ack;
    seq_t next_seq;
    Vtimer<seq_t> timer;
    queue<packet> buffer;

    packet* pop() {
        in_window--;
        packet *ret = window + (expected_ack % S);
        inc_circularly(expected_ack);
        return ret;
    }

    bool available() {
        return in_window < S;
    }

    int count() {
        return in_window;
    }

public:
    void AckUpTo(seq_t actual_ack) {
        debug_printf("AckUpTo: expected_ack %d, actual_ack %d, next_seq %d", expected_ack, actual_ack, next_seq);
        while (between(expected_ack, actual_ack, next_seq)) {
            timer.Stop(expected_ack);
            pop();
        }
        while (available() && !buffer.empty()) {
            packet p = buffer.front();
            buffer.pop();
            Push(&p);
        }
    }

    void Push(packet *pkt) {
        if (available()) {
            seq_t seq = *(ref_seq(pkt));
            debug_printf("Push: send seq %d", seq);

            in_window++;
            inc_circularly(next_seq);
            window[seq % S] = *pkt;
            timer.Start(seq, DEFAULT_TIMEOUT);
            Sender_ToLowerLayer(pkt);
        } else {
            buffer.push(*pkt);
        }
    }

    void Timeout() {
        timer.Timeout();
        debug_printf("Timeout: count %d, expected_ack %d", count(), expected_ack);
        seq_t memento = expected_ack;
        next_seq = expected_ack;
        for (int i = 0; i < count(); ++i) {
            Push(pop());
        }
        expected_ack = memento;
    }
};

#define WINDOW_SIZE 8

Window<8> window = Window<8>();

// extra sequence num in order not to block upper layer
static seq_t pack_seq = 0;

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
    window.Push(&pkt);

	/* move the cursor */
	cursor += MAX_PLS;
    }

    /* send out the last packet */
    if (msg->size > cursor) {
	/* fill in the packet */
	pack(&pkt, pack_seq, msg->data+cursor, msg->size-cursor);
    inc_circularly(pack_seq);

	/* send it out through the lower layer */
    window.Push(&pkt);
    }
}

/* event handler, called when a packet is passed from the lower layer at the 
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
    if (!sanity_check(pkt)) {
        return;
    }

    window.AckUpTo(*(ref_seq(pkt)));
}

/* event handler, called when the timer expires */
void Sender_Timeout()
{
    window.Timeout();
}

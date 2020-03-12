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

#include <set>
#include <functional>
#include <iterator>

using namespace std;

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

static void pack(packet *pkt, seq_t seq, bool is_nak) {
    memset(pkt->data+HEADER_SIZE, 0, ACK_PLS);
    if (is_nak) {
        nak(pkt);
    }
    *(ref_payload_size(pkt)) = ACK_PLS;
    *(ref_seq(pkt)) = seq;
    *(ref_checksum(pkt)) = sum(pkt);
}

template<int S>
class Handler
{
protected:
    seq_t expected_seq;

    void upload(int size, function<void(message*)> fill) {
        if (size <= 0) {
            return;
        }
        message *msg = (message *)malloc(sizeof(message));
        msg->size = size;
        msg->data = (char *)malloc(size);
        fill(msg);
        Receiver_ToUpperLayer(msg);
        if (msg->data != NULL) free(msg->data);
        if (msg != NULL) free(msg);
    }
    
    void ack(seq_t seq, bool is_nak) {
        packet rsp;
        pack(&rsp, seq, is_nak);
        Receiver_ToLowerLayer(&rsp);
    }
public:
    virtual void Handle(packet *pkt) = 0;
    seq_t Expect() {
        return expected_seq;
    }
};

template<int S>
class SRHandler : public Handler<S>
{
private:
    struct comparator {
        bool operator() (const packet &a, const packet &b) {
            return less_than(get_seq(&a), get_seq(&b), S);
        }
    };

    set<packet, comparator> buffer;

    void flush() {
        int size = 0;
        auto last = buffer.begin();
        for (; last != buffer.end(); ++last) {
            seq_t seq = get_seq(&(*last));
            if (seq != this->expected_seq) {
                break;
            }
            this->ack(this->expected_seq, false);
            size += get_payload_size(&(*last));
            inc_circularly(this->expected_seq);
        }
        debug_printf("flush: %d packets", distance(buffer.begin(), last));
        auto fill = [&](message *msg) {
            int cursor = 0;
            for (auto it = buffer.begin(); it != last; ++it) {
                pls_t payload_size = get_payload_size(&(*it));
                debug_printf("fill msg, seq %d, pls %d", get_seq(&(*it)), get_payload_size(&(*it)));
                memcpy(msg->data+cursor, it->data+HEADER_SIZE, payload_size);
                cursor += payload_size;
            }
        };
        this->upload(size, fill);
        buffer.erase(buffer.begin(), last);
    }

    void push(packet *pkt) {
        debug_printf("push: seq %d", get_seq(pkt));
        buffer.insert(*pkt);
        if (get_seq(pkt) == this->expected_seq) {
            debug_printf("push: trigger flush");
            flush();
        }
    }
public:
    void Handle(packet *pkt) {
        seq_t seq = get_seq(pkt);
        if (seq == this->expected_seq) {
            debug_printf("Handle: seq %d as expected", seq);
            push(pkt);
        } else if (less_than(this->expected_seq, seq, S)) {
            debug_printf("Handle: fresher seq %d, send nak of %d", seq, this->expected_seq);
            this->ack(this->expected_seq, true);
            push(pkt);
        } else if (less_than(seq, this->expected_seq, S)) {
            debug_printf("Handle: stale seq %d, resend ack", seq);
            this->ack(seq, false);
            return;
        } else {
            debug_printf("Handle: seq %d outside of window, drop", seq);
            return;
        }
    }
};

template<int S>
class GBNHandler : public Handler<S>
{
private:
    void push(packet *pkt) {
        debug_printf("push: seq %d", get_seq(pkt));
        this->ack(this->expected_seq, false);
        inc_circularly(this->expected_seq);

        int size = get_payload_size(pkt);
        auto fill = [&](message *msg) {
            memcpy(msg->data, pkt->data+HEADER_SIZE, size);
        };
        this->upload(size, fill);
    }
public:
    void Handle(packet *pkt) {
        seq_t seq = get_seq(pkt);
        if (seq == this->expected_seq) {
            debug_printf("Handle: seq %d as expected", seq);
            push(pkt);
        } else if (less_than(seq, this->expected_seq, S)) {
            debug_printf("Handle: stale seq %d, resend ack", seq);
            this->ack(seq, false);
            return;
        }
    }
};

#if GBN
Handler<REC_WINDOW_SIZE> *handler = new GBNHandler<REC_WINDOW_SIZE>();
#else
Handler<REC_WINDOW_SIZE> *handler = new SRHandler<REC_WINDOW_SIZE>();
#endif

/* event handler, called when a packet is passed from the lower layer at the 
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt)
{
    /* 1-byte header indicating the size of the payload */
    // int header_size = sizeof(packet_header);

    pls_t payload_size = get_payload_size(pkt);
    checksum_t checksum = get_checksum(pkt);
    seq_t seq = get_seq(pkt);
    
    debug_printf("Receiver_FromLowerLayer: seq %d, pls %d, checksum %d, expected %d", seq, payload_size, checksum, handler->Expect());

    if (!sanity_check(pkt)) {
        debug_printf("Receiver_FromLowerLayer: drop seq %d", seq);
        return;
    }

    handler->Handle(pkt);
}

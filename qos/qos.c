#include "qos.h"
#include <stdint.h>
#include "rte_meter.h"
#include "rte_red.h"
#include <stdio.h>

static struct rte_meter_srtcm meter[APP_FLOWS_MAX];

static uint64_t init_tsc;

static uint64_t tsc_frequency;

#define GbpsToBytes(x) ((double)(x) * 1000 * 1000 * 1000 / 8) 

/**
 * This function will be called only once at the beginning of the test. 
 * You can initialize your meter here.
 * 
 * int rte_meter_srtcm_config(struct rte_meter_srtcm *m, struct rte_meter_srtcm_params *params);
 * @return: 0 upon success, error code otherwise
 * 
 * void rte_exit(int exit_code, const char *format, ...)
 * #define rte_panic(...) rte_panic_(__func__, __VA_ARGS__, "dummy")
 * 
 * uint64_t rte_get_tsc_hz(void)
 * @return: The frequency of the RDTSC timer resolution
 * 
 * static inline uint64_t rte_get_tsc_cycles(void)
 * @return: The time base for this lcore.
 */
int
qos_meter_init(void)
{
    init_tsc = rte_get_tsc_cycles();
    printf("init_tsc: %ld\n", init_tsc);

    tsc_frequency = rte_get_tsc_hz();
    printf("tsc_frequency: %ld\n", tsc_frequency);

    uint64_t ar = GbpsToBytes(1.28);
    uint64_t cir0 = ar;
    uint64_t cir1 = cir0 / 2;
    uint64_t cir2 = cir1 / 2;
    uint64_t cir3 = cir2 / 2;

    double rtt = 0.001;

    struct rte_meter_srtcm_params srtcm_params[APP_FLOWS_MAX] = {
        {.cir = cir0, .cbs = cir0 * rtt, .ebs = cir0 * rtt},
        {.cir = cir1, .cbs = cir1 * rtt, .ebs = cir1 * rtt},
        {.cir = cir2, .cbs = cir2 * rtt, .ebs = cir2 * rtt},
        {.cir = cir3, .cbs = cir3 * rtt, .ebs = cir3 * rtt}
    };

    int ret;

    for (int i = 0; i < APP_FLOWS_MAX; i++) {
        ret = rte_meter_srtcm_config(&meter[i], &srtcm_params[i]);
        if (ret != 0) {
            rte_panic("failed to init meter");
        }
    }

    return 0;
}

#define NANO_PER_S 1000000000

/**
 * This function will be called for every packet in the test, 
 * after which the packet is marked by returning the corresponding color.
 * 
 * A packet is marked green if it doesn't exceed the CBS, 
 * yellow if it does exceed the CBS, but not the EBS, and red otherwise
 * 
 * The pkt_len is in bytes, the time is in nanoseconds.
 * 
 * Point: We need to convert ns to cpu circles
 * Point: Time is not counted from 0
 * 
 * static inline enum rte_meter_color rte_meter_srtcm_color_blind_check(struct rte_meter_srtcm *m,
	uint64_t time, uint32_t pkt_len)
 * 
 * enum qos_color { GREEN = 0, YELLOW, RED };
 * enum rte_meter_color { e_RTE_METER_GREEN = 0, e_RTE_METER_YELLOW,  
	e_RTE_METER_RED, e_RTE_METER_COLORS };
 */ 
enum qos_color
qos_meter_run(uint32_t flow_id, uint32_t pkt_len, uint64_t time)
{
    uint64_t tsc = init_tsc + (tsc_frequency + NANO_PER_S - 1) / NANO_PER_S * time;
    return rte_meter_srtcm_color_blind_check(&meter[flow_id], tsc, pkt_len);
}

static struct rte_red dropper;
static struct rte_red_config dropper_configs[APP_FLOWS_MAX][e_RTE_METER_COLORS];

/**
 * This function will be called only once at the beginning of the test. 
 * You can initialize you dropper here
 * 
 * int rte_red_rt_data_init(struct rte_red *red);
 * @return Operation status, 0 success
 * 
 * int rte_red_config_init(struct rte_red_config *red_cfg, const uint16_t wq_log2, 
   const uint16_t min_th, const uint16_t max_th, const uint16_t maxp_inv);
 * @return Operation status, 0 success 
 */
int
qos_dropper_init(void)
{
    struct rte_red_params red_params[APP_FLOWS_MAX][e_RTE_METER_COLORS] = {
        {
            {.min_th = 250, .max_th = 500, .maxp_inv = 10, .wq_log2 = 9}, // GREEN
            {.min_th = 25, .max_th = 75, .maxp_inv = 5, .wq_log2 = 9}, // YELLOW
            {.min_th = 25, .max_th = 75, .maxp_inv = 2, .wq_log2 = 9} // RED
        },
        {
            {.min_th = 250, .max_th = 500, .maxp_inv = 10, .wq_log2 = 9}, // GREEN
            {.min_th = 25, .max_th = 75, .maxp_inv = 5, .wq_log2 = 9}, // YELLOW
            {.min_th = 25, .max_th = 75, .maxp_inv = 2, .wq_log2 = 9} // RED
        },
        {
            {.min_th = 250, .max_th = 500, .maxp_inv = 10, .wq_log2 = 9}, // GREEN
            {.min_th = 25, .max_th = 75, .maxp_inv = 5, .wq_log2 = 9}, // YELLOW
            {.min_th = 25, .max_th = 75, .maxp_inv = 2, .wq_log2 = 9} // RED
        },
        {
            {.min_th = 250, .max_th = 500, .maxp_inv = 10, .wq_log2 = 9}, // GREEN
            {.min_th = 25, .max_th = 75, .maxp_inv = 5, .wq_log2 = 9}, // YELLOW
            {.min_th = 25, .max_th = 75, .maxp_inv = 2, .wq_log2 = 9} // RED
        },
    };

    int ret;

    ret = rte_red_rt_data_init(&dropper);
    if (ret != 0) {
        rte_panic("failed to init dropper");
    }
    for (int i = 0; i < APP_FLOWS_MAX; i++) {
        for (int j = 0; j < e_RTE_METER_COLORS; j++) {
            struct rte_red_params *p = &red_params[i][j];
            ret = rte_red_config_init(&dropper_configs[i][j], p->wq_log2, p->min_th, p->max_th, p->maxp_inv);
            if (ret != 0) {
                rte_panic("failed to init dropper");
            }
        }
    }
     
    return 0;
}

static unsigned q_size = 0;

#define NOT_ZERO 13123123

static uint64_t last_time = NOT_ZERO;

/**
 * This function will be called for every tested packet after being marked by the meter, 
 * and will make the decision whether to drop the packet by returning the decision (0 pass, 1 drop)
 * 
 * The probability of drop increases as the estimated average queue size grows
 * 
 * static inline void rte_red_mark_queue_empty(struct rte_red *red, const uint64_t time)
 * @brief Callback to records time that queue became empty
 * @param q_time : Start of the queue idle time (q_time) 
 * 
 * static inline int rte_red_enqueue(const struct rte_red_config *red_cfg,
	struct rte_red *red, const unsigned q, const uint64_t time)
 * @param q [in] updated queue size in packets   
 * @return Operation status
 * @retval 0 enqueue the packet
 * @retval 1 drop the packet based on max threshold criteria
 * @retval 2 drop the packet based on mark probability criteria
 */
int
qos_dropper_run(uint32_t flow_id, enum qos_color color, uint64_t time)
{
    uint64_t tsc = init_tsc + (tsc_frequency + NANO_PER_S - 1) / NANO_PER_S * time;
    struct rte_red_config *config = &dropper_configs[flow_id][color];

    if (time != last_time) {
        rte_red_mark_queue_empty(&dropper, tsc);
        printf("last q_size: %d\n", q_size);
        q_size = 0;
        last_time = time;
    } 
    
    q_size++;
    int ret = rte_red_enqueue(config, &dropper, q_size, tsc);
    if (ret != 0) {
        q_size--;
    }
    return ret;
}

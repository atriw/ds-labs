#include "rte_common.h"
#include "rte_mbuf.h"
#include "rte_meter.h"
#include "rte_red.h"

#include "qos.h"

struct rte_meter_srtcm app_flows[APP_FLOWS_MAX];

/**
 * srTCM
 */
int
qos_meter_init(void)
{
    /* to do */
    struct rte_meter_srtcm_params app_srtcm_params[APP_FLOWS_MAX] = {
        { .cir=160000000000 , .cbs=80000, .ebs=80000 },
        { .cir=80000000000 , .cbs=40000, .ebs=40000 },
        { .cir=40000000000 , .cbs=20000, .ebs=20000 },
        { .cir=20000000000 , .cbs=10000, .ebs=10000 }
    };
    int ret;

    for (int i = 0; i < APP_FLOWS_MAX; i++) {
        ret = rte_meter_srtcm_config(&app_flows[i], &app_srtcm_params[i]);
        if (ret)
            return ret;
    }
    return 0;
}

enum qos_color
qos_meter_run(uint32_t flow_id, uint32_t pkt_len, uint64_t time)
{
    /* to do */
    return rte_meter_srtcm_color_blind_check(&app_flows[flow_id], time, pkt_len);
}


/**
 * WRED
 */

struct rte_red_config app_red_config[APP_FLOWS_MAX][e_RTE_METER_COLORS];
struct rte_red app_red_data[APP_FLOWS_MAX][e_RTE_METER_COLORS];
unsigned qsize[APP_FLOWS_MAX];
uint64_t last_time;

int
qos_dropper_init(void)
{
    /* to do */
    struct rte_red_params app_red_params[APP_FLOWS_MAX][e_RTE_METER_COLORS] = {
        {
            {.min_th = 1022, .max_th = 1023, .maxp_inv = 10, .wq_log2 = 9},
            {.min_th = 1022, .max_th = 1023, .maxp_inv = 10, .wq_log2 = 9},
            {.min_th = 0, .max_th = 1, .maxp_inv = 10, .wq_log2 = 9}
        },
        {
            {.min_th = 1022, .max_th = 1023, .maxp_inv = 10, .wq_log2 = 9},
            {.min_th = 1022, .max_th = 1023, .maxp_inv = 10, .wq_log2 = 9},
            {.min_th = 0, .max_th = 1, .maxp_inv = 10, .wq_log2 = 9}
        },
        {
            {.min_th = 1022, .max_th = 1023, .maxp_inv = 10, .wq_log2 = 9},
            {.min_th = 1022, .max_th = 1023, .maxp_inv = 10, .wq_log2 = 9},
            {.min_th = 0, .max_th = 1, .maxp_inv = 10, .wq_log2 = 9}
        },
        {
            {.min_th = 1022, .max_th = 1023, .maxp_inv = 10, .wq_log2 = 9},
            {.min_th = 1022, .max_th = 1023, .maxp_inv = 10, .wq_log2 = 9},
            {.min_th = 0, .max_th = 1, .maxp_inv = 10, .wq_log2 = 9}
        }
    };
    for (int i = 0; i < APP_FLOWS_MAX; i++) {
        for (int j = 0; j < e_RTE_METER_COLORS; j++) {
            rte_red_rt_data_init(&app_red_data[i][j]);
            rte_red_config_init(&app_red_config[i][j], 
            app_red_params[i][j].wq_log2, 
            app_red_params[i][j].min_th,
            app_red_params[i][j].max_th,
            app_red_params[i][j].maxp_inv);
        }
        qsize[i] = 0;
    }
}

int
qos_dropper_run(uint32_t flow_id, enum qos_color color, uint64_t time)
{
    /* to do */
    if (time != last_time) {
        for (int i = 0; i < APP_FLOWS_MAX; i++) {
            for (int j = 0; j < e_RTE_METER_COLORS; j++) {
                rte_red_mark_queue_empty(&app_red_data[i][j], time);
            }
            qsize[i] = 0;
        }
    }
    last_time = time;
    int ret;
    if ((ret = rte_red_enqueue(&app_red_config[flow_id][color], &app_red_data[flow_id][color], time)) == 0)
        qsize[flow_id]++;
    return ret;
}
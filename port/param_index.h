/*
 * param_index.h
 *
 * Change Logs:
 * Date           Author            Notes
 * 2020-06-07     qiyongzhong       first version
 */

#ifndef __PARAM_INDEX_H__
#define __PARAM_INDEX_H__

/* Param index definition
 * Note: the index must begin at 0
 * Order must be consistent with parameter definition
 */
typedef enum{
    PIDX_CAR = 0,           //
    PIDX_MAC_ADDR,          //
    PIDX_MY_AGE,            //
    PIDX_MY_MONEY,          //
    PIDX_REG_ADDR,          //
    PIDX_REG_VALUE,         //
    PIDX_VOLTAGE,           //
    PIDX_ENERGY,            //
    
    PIDX_TOTAL              //param total
}param_idx_t;

#endif


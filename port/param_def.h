/*
 * param_def.h
 *
 * Change Logs:
 * Date           Author            Notes
 * 2020-06-07     qiyongzhong       first version
 */

#ifndef __PARAM_DEF_H__
#define __PARAM_DEF_H__

#ifdef PARAM_TABLE_DEF

/*
//parameter definition sample
//------------------------------
//param definition begin
PARAM_BEGIN()

//Please define your parameter items here  
PARAM_STRING(car,         15,     wow)
PARAM_ARRAY (mac_addr,    6,      AB-CD-EF-01-02-03)
PARAM_INT   (my_age,      25)
PARAM_INT64 (my_money,    56789123456789)
PARAM_HEX   (reg_addr,    A001)
PARAM_HEX64 (reg_value,   12345678ABCDEF)
PARAM_FLOAT (voltage,     12.34)
PARAM_DOUBLE(energy,      87654321.123)

//param definition end
PARAM_END()
//------------------------------
*/


//parameter definition sample

//param definition begin
PARAM_BEGIN()

//Please define your parameter items here  
PARAM_STRING(car,         15,     wow)
PARAM_ARRAY (mac_addr,    6,      AB CD EF 01 02 03)
PARAM_INT   (my_age,      25)
PARAM_INT64 (my_money,    56789123456789)
PARAM_HEX   (reg_addr,    A001)
PARAM_HEX64 (reg_value,   12345678ABCDEF)
PARAM_FLOAT (voltage,     12.34)
PARAM_DOUBLE(energy,      87654321.123)

//param definition end
PARAM_END()

#endif
#endif


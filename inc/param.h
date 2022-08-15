/*
 * param.h
 *
 * Change Logs:
 * Date           Author            Notes
 * 2020-06-07     qiyongzhong       first version
 */

#ifndef __PARAM_H__
#define __PARAM_H__

#include <typedef.h>
#include <rtconfig.h>
#include <param_index.h>

//#define PKG_USING_QPARAM
//#define PARAM_USING_INDEX       //using index fast read/write param
//#define PARAM_USING_CLI         //using command line list/read/write... param
//#define PARAM_USING_AUTO_INIT   //using automatic initialize and load from flash
//#define PARAM_USING_AUTO_SAVE   //using automatic save into flash

#ifndef PARAM_AUTO_SAVE_DELAY
#define PARAM_AUTO_SAVE_DELAY   2000
#endif

#ifndef PARAM_PART_NAME
#define PARAM_PART_NAME         "param" //flash partition name for saving parameters
#endif

#ifndef PARAM_SECTOR_SIZE
#define PARAM_SECTOR_SIZE       4096    //flash sector size
#endif

#ifndef PARAM_SAVE_ADDR
#define PARAM_SAVE_ADDR         0       //save address for parameters 
#endif

#ifndef PARAM_SAVE_ADDR_BAK
#define PARAM_SAVE_ADDR_BAK     (PARAM_SAVE_ADDR + PARAM_SECTOR_SIZE)//save address for backup parameters 
#endif

#define PARAM_MAGIC_WORD        0xCC33

/* 
 * @brief   initialize parameter module
 * @param   none
 * @retval  0 - success, <0 - error
 */
int param_init(void);

/* 
 * @brief   deinitialize parameter module
 * @param   none
 * @retval  none
 */
void param_deinit(void);

/* 
 * @brief   load parameter from flash
 * @param   none
 * @retval  0 - success, <0 - error
 */
int param_load_from_flash(void);

/* 
 * @brief   save parameter to flash
 * @param   none
 * @retval  0 - success, <0 - error
 */
int param_save_to_flash(void);

/* 
 * @brief   resume all parameter to default
 * @param   none
 * @retval  0 - success, <0 - error
 */
int param_resume_all(void);

/* 
 * @brief   resume default by name
 * @param   name - parameter name
 * @retval  0 - success, <0 - error
 */
int param_resume_by_name(char *name);

/* 
 * @brief   read parameter by name
 * @param   name - parameter name
 * @param   addr - address of the variable that save parameter 
 * @param   size - size of the variable that save parameter
 * @retval  0 - success, <0 - error
 */
int param_read_by_name(char *name, void *addr, int size);

/* 
 * @brief   write parameter by name
 * @param   name - parameter name
 * @param   addr - address of the variable that save parameter 
 * @param   size - size of the variable that save parameter
 * @retval  0 - success, <0 - error
 */
int param_write_by_name(char *name, const void *addr, int size);

#ifdef PARAM_USING_INDEX

/* 
 * @brief   get parameter name by index
 * @param   idx - parameter index
 * @retval  pointer to parameter name , NULL - parameter is not exist
 */
const char *param_get_name(int idx);

/* 
 * @brief   resume default by index
 * @param   idx - parameter index
 * @retval  0 - success, <0 - error
 */
int param_resume_by_index(int idx);

/* 
 * @brief   read parameter by name
 * @param   idx - parameter index
 * @param   addr - address of the variable that save parameter 
 * @param   size - size of the variable that save parameter
 * @retval  0 - success, <0 - error
 */
int param_read_by_index(int idx, void *addr, int size);

/* 
 * @brief   write param by index
 * @param   idx - parameter index
 * @param   addr - address of the variable that save parameter 
 * @param   size - size of the variable that save parameter
 * @retval  0 - success, <0 - error
 */
int param_write_by_index(int idx, const void *addr, int size);

#endif
#endif


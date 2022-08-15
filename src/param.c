/*
 * param.c
 *
 * Change Logs:
 * Date           Author            Notes
 * 2020-06-07     qiyongzhong       first version
 */

#include <rtthread.h>
#include <fal.h>
#include <crc16.h>
#include <param.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define DBG_TAG "param"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define PARAM_CRC16_CAL(p, l)                   crc16_cal(p, l)

#define PARAM_MUTEX_CREATE()                    rt_mutex_create("param", RT_IPC_FLAG_FIFO)
#define PARAM_MUTEX_DELETE(p)                   rt_mutex_delete(p)
#define PARAM_MUTEX_TAKE(p)                     rt_mutex_take(p, RT_WAITING_FOREVER)
#define PARAM_MUTEX_RELEASE(p)                  rt_mutex_release(p)

#define PARAM_FLASH_FIND(name)                  fal_partition_find(name)
#define PARAM_FLASH_ERASE(p, addr, size)        fal_partition_erase(p, addr, size)
#define PARAM_FLASH_READ(p, addr, buf, size)    fal_partition_read(p, addr, buf, size)
#define PARAM_FLASH_WRITE(p, addr, buf, size)   fal_partition_write(p, addr, buf, size)

#define PARAM_PRINT                             rt_kprintf

typedef enum{
    PTYPE_STR = 0,      //0-string
    PTYPE_ARRAY,        //1-unsigned char array
    PTYPE_INT,          //2-32/64 bits signed int
    PTYPE_HEX,          //3-32/64 bits unsigned int
    PTYPE_FLOAT         //4-float/double
}param_type_t;

typedef struct
{
    u16 magic;
    u16 size;
    u16 crc16;
    u16 head_crc16;
}param_head_t;

typedef struct
{
    char *name;
    char *defval;
    u8  type;
    u8  size;
}param_msg_t;

#define PARAM_BEGIN()   static const param_msg_t param_msg_table[] = {
#define PARAM_END()     };

#define PARAM_STRING(name, size, defval)    {#name, #defval,    PTYPE_STR,      size+1},
#define PARAM_ARRAY(name, size, defval)     {#name, #defval,    PTYPE_ARRAY,    size},
#define PARAM_INT(name, defval)             {#name, #defval,    PTYPE_INT,      sizeof(u32)},
#define PARAM_INT64(name, defval)           {#name, #defval,    PTYPE_INT,      sizeof(u64)},
#define PARAM_HEX(name, defval)             {#name, #defval,    PTYPE_HEX,      sizeof(u32)},
#define PARAM_HEX64(name, defval)           {#name, #defval,    PTYPE_HEX,      sizeof(u64)},
#define PARAM_FLOAT(name, defval)           {#name, #defval,    PTYPE_FLOAT,    sizeof(f32)},
#define PARAM_DOUBLE(name, defval)          {#name, #defval,    PTYPE_FLOAT,    sizeof(f64)},

#define PARAM_TABLE_DEF
#include <param_def.h>

#define PARAM_TOTAL                         (sizeof(param_msg_table)/sizeof(param_msg_table[0]))

static fal_partition_t part = NULL;
static rt_mutex_t param_mutex = NULL;
static u8 *param_datas = NULL;
static u16 param_size;
static param_head_t param_head;
static u16 param_offset_table[PARAM_TOTAL];

#ifdef PARAM_USING_AUTO_SAVE
static rt_timer_t param_auto_save_timer = NULL;
#endif

static void param_size_init(void)
{
    int size = 0;

    for (int i = 0; i < PARAM_TOTAL; i++)
    {
        param_offset_table[i] = size;
        size += param_msg_table[i].size;
    }

    param_size = size;
}

static int param_datas_init(void)
{
    param_size_init();
    
    if ((param_size > 0) && (param_datas == NULL))
    {
        param_datas = malloc(param_size);
    }
    
    return ((param_datas != NULL) ? RT_EOK : -RT_ENOMEM);
}

static void param_datas_deinit(void)
{
    if (param_datas != NULL)
    {
        free(param_datas);
        param_datas = NULL;
    }
}

static int param_mutex_init(void)
{
    if (param_mutex == NULL)
    {
        param_mutex = PARAM_MUTEX_CREATE();
    }
    
    return ((param_mutex != NULL) ? RT_EOK : -RT_ENOMEM);
}

static void param_mutex_deinit(void)
{
    if (param_mutex != NULL)
    {
        PARAM_MUTEX_DELETE(param_mutex);
        param_mutex = NULL;
    }
}

static void param_mutex_take(void)
{
    PARAM_MUTEX_TAKE(param_mutex);
}

static void param_mutex_release(void)
{
    PARAM_MUTEX_RELEASE(param_mutex);
}

static int param_part_init(void)
{
    if (part == NULL)
    {
        part = (fal_partition_t)PARAM_FLASH_FIND(PARAM_PART_NAME);
    }
    
    return ((part != NULL) ? RT_EOK : -RT_ENOMEM);
}

#ifdef PARAM_USING_AUTO_SAVE
static int param_auto_save_timer_init(void)
{
    if (param_auto_save_timer == NULL)
    {
        param_auto_save_timer = rt_timer_create("par_save", 
                                                (void(*)(void*))param_save_to_flash,
                                                NULL, 
                                                PARAM_AUTO_SAVE_DELAY, 
                                                (RT_TIMER_FLAG_ONE_SHOT | RT_TIMER_FLAG_SOFT_TIMER));
    }
    
    return ((param_auto_save_timer != NULL) ? RT_EOK : -RT_ENOMEM);
}

static void param_auto_save_timer_deinit(void)
{
    if (param_auto_save_timer != NULL)
    {
        rt_timer_delete(param_auto_save_timer);
        param_auto_save_timer = NULL;
    }
}

static void param_auto_save_start(void)
{
    rt_timer_start(param_auto_save_timer);
}

static void param_auto_save_stop(void)
{
    rt_timer_stop(param_auto_save_timer);
}
#endif

static void param_head_update(void)
{
    param_head.magic = PARAM_MAGIC_WORD;
    param_head.size = param_size;
    param_head.crc16 = PARAM_CRC16_CAL((u8*)param_datas, param_size);
    param_head.head_crc16 = PARAM_CRC16_CAL((u8*)&param_head, sizeof(param_head)-2);
}

static int param_head_check(void)
{
    if (param_head.magic != PARAM_MAGIC_WORD)
    {
        return(-RT_ERROR);
    }
    if (PARAM_CRC16_CAL((u8*)&param_head, sizeof(param_head)-2) != param_head.head_crc16)
    {
        return(-RT_ERROR);
    }
    return(RT_EOK);
}

static int param_check(void)
{
    if (PARAM_CRC16_CAL(param_datas, param_head.size) != param_head.crc16)
    {
        return(-RT_ERROR);
    }
    return(RT_EOK);
}

static int param_write_to_addr(u32 addr)
{
    if (PARAM_FLASH_ERASE(part, addr, PARAM_SECTOR_SIZE) < 0)
    {
        LOG_E("param sector erease fail. addr : %d", addr);
        return(-RT_ERROR);
    }
    if (PARAM_FLASH_WRITE(part, addr, (u8*)&param_head, sizeof(param_head)) < 0)
    {
        LOG_E("param head write fail. addr : %d", addr);
        return(-RT_ERROR);
    }
    if (PARAM_FLASH_WRITE(part, addr+sizeof(param_head), (u8*)param_datas, param_size) < 0)
    {
        LOG_E("param write fail. addr : %d", addr);
        return(-RT_ERROR);
    }
    LOG_D("param write success. addr : %d", addr);
    return(RT_EOK);
}

static int param_read_from_addr(u32 addr)
{
    if (PARAM_FLASH_READ(part, addr, (u8*)&param_head, sizeof(param_head)) < 0)
    {
        LOG_E("param head read fail. addr : %d", addr);
        return(-RT_ERROR);
    }
    if (param_head_check() < 0)
    {
        LOG_E("param head check fail. addr : %d", addr);
        return(-RT_ERROR);
    }
    if (param_head.size > param_size)
    {
        LOG_E("param size check fail. addr : %d", addr);
        return(-RT_ERROR);
    }
    if (PARAM_FLASH_READ(part, addr+sizeof(param_head), (u8*)param_datas, param_head.size) < 0)
    {
        LOG_E("param read fail. addr : %d", addr);
        return(-RT_ERROR);
    }
    if (param_check() < 0)
    {
        LOG_E("param check fail. addr : %d", addr);
        return(-RT_ERROR);
    }
    LOG_D("param read success. addr : %d", addr);
    return(RT_EOK);
}

static param_type_t param_get_type(int idx)
{
    return(param_msg_table[idx].type);
}

static int param_get_size(int idx)
{
    return(param_msg_table[idx].size);
}

static int param_find_by_name(const char *name)
{
    for (int i=0; i<PARAM_TOTAL; i++)
    {
        if (strcmp(param_msg_table[i].name, name) == 0)
        {
            return(i);
        }
    }
    return(-1);
}

static int param_input_value(void *buf, int type, int size, const char *input_str)
{
    switch(type)
    {
    case PTYPE_STR:
        strncpy(buf, input_str, size-1);
        break;
    case PTYPE_ARRAY:
        {
            u8 *pbyte = buf;
            char *p = (char *)input_str;
            char *pn = NULL;
            while(1)
            {
                *pbyte++ = strtol(p, &pn, 16);
                if (*pn == 0) break;
                p = pn+1;
            }
        }
        break;
    case PTYPE_INT:
        {
            s64 lval = strtoll(input_str, NULL, 0);
            memcpy(buf, (u8 *)&lval, size);
        }
        break;
    case PTYPE_HEX:
        {
            u64 lval = strtoll(input_str, NULL, 16);
            memcpy(buf, (u8 *)&lval, size);
        }
        break;
    case PTYPE_FLOAT:
        {
            if (size == sizeof(f32))
            {
                f32 fval = atof(input_str);
                memcpy(buf, (u8 *)&fval, size);
            }
            else
            {
                f64 fval = atof(input_str);
                memcpy(buf, (u8 *)&fval, size);
            }
        }
        break;
    default:
        return(0);
    }
    
    return(size);
}

static int _param_resume_all(void)
{
    if (param_datas == NULL || param_mutex == NULL)
    {
        LOG_E("param resume all fail. param no initialized.");
        return(-RT_ERROR);
    }
    
    param_mutex_take();
    memset(param_datas, 0, param_size);
    for (int i = 0; i < PARAM_TOTAL; i++)
    {
        int type = param_msg_table[i].type;
        int size = param_msg_table[i].size;
        char *str = param_msg_table[i].defval;
        u8 *paddr = param_datas + param_offset_table[i];

        param_input_value(paddr, type, size, str);
    }
    param_mutex_release();

    return(RT_EOK);
}

int param_init(void)
{
    param_deinit();
    
    if (param_part_init() != RT_EOK)
    {
        LOG_E("param fal partition initialize fail.");
        return(-RT_ERROR);
    }
    
    if (param_mutex_init() != RT_EOK)
    {
        LOG_E("param mutex init error. no memory for create mutex.");
        return(-RT_ERROR);
    }
    
    if (param_datas_init() != RT_EOK)
    {
        param_mutex_deinit();
        LOG_E("param datas init error. no memory for create param datas.");
        return(-RT_ERROR);
    }
    
    #ifdef PARAM_USING_AUTO_SAVE
    if (param_auto_save_timer_init() != RT_EOK)
    {
        param_mutex_deinit();
        param_datas_deinit();
        LOG_E("param datas init error. no memory for create param datas.");
        return(-RT_ERROR);
    }
    #endif
    
    _param_resume_all();
    
    return(RT_EOK);
}

void param_deinit(void)
{
    param_mutex_deinit();
    param_datas_deinit();
    #ifdef PARAM_USING_AUTO_SAVE
    param_auto_save_timer_deinit();
    #endif
}

int param_load_from_flash(void)
{
    int rst;
    
    if (part == NULL || param_datas == NULL || param_mutex == NULL)
    {
        LOG_E("param load failed. param no initialized.");
        return(-RT_ERROR);
    }

    param_mutex_take();
    rst = param_read_from_addr(PARAM_SAVE_ADDR);
    param_mutex_release();
    
    if (rst == RT_EOK)
    {
        LOG_D("param load success from flash partition.");
        return(RT_EOK);
    }
    
    param_mutex_take();
    rst = param_read_from_addr(PARAM_SAVE_ADDR_BAK);
    param_mutex_release();
    
    if (rst == RT_EOK)
    {
        LOG_D("param load success from flash backup partition.");
        return(RT_EOK);
    }

    LOG_E("param load failed .");
    return(-RT_ERROR);
}

int param_save_to_flash(void)
{
    int rst1, rst2;
    
    if (part == NULL || param_datas == NULL || param_mutex == NULL)
    {
        LOG_E("param save failed . param no initialized.");
        return(-RT_ERROR);
    }
    
    param_mutex_take();
    param_head_update();
    rst1 = param_write_to_addr(PARAM_SAVE_ADDR);
    rst2 = param_write_to_addr(PARAM_SAVE_ADDR_BAK);
    param_mutex_release();

    #ifdef PARAM_USING_AUTO_SAVE
    param_auto_save_stop();
    #endif
    
    if ((rst1 != RT_EOK) && (rst2 != RT_EOK))
    {
        LOG_E("param save failed . param write flash error.");
        return(-RT_ERROR);
    }
    
    return(RT_EOK);
}

int param_resume_all(void)
{
    int rst = _param_resume_all();
    
    #ifdef PARAM_USING_AUTO_SAVE
    if (rst == RT_EOK)
    {
        param_auto_save_start();
    }
    #endif

    return(rst);
}

const char *param_get_name(int idx)
{
    if ((u32)idx >= PARAM_TOTAL)
    {
        return(NULL);
    }
    return(param_msg_table[idx].name);
}

int param_resume_by_index(int idx)
{
    if (param_datas == NULL || param_mutex == NULL)
    {
        LOG_E("param resume fail by index. param no initialized.");
        return(-RT_ERROR);
    }
    
    if ((u32)idx < PARAM_TOTAL)
    {
        int type = param_msg_table[idx].type;
        int size = param_msg_table[idx].size;
        char *str = param_msg_table[idx].defval;
        u8 *paddr = param_datas + param_offset_table[idx];
        
        param_mutex_take();
        param_input_value(paddr, type, size, str);
        param_mutex_release();

        #ifdef PARAM_USING_AUTO_SAVE
        param_auto_save_start();
        #endif

        return(RT_EOK);
    }

    return(-RT_ERROR);
}

int param_read_by_index(int idx, void *addr, int size)
{
    param_type_t ptype;
    int psize;
    u8 *paddr;
    
    if (param_datas == NULL || param_mutex == NULL)
    {
        LOG_E("param read fail by index. param no initialized.");
        return(-RT_ERROR);
    }
    
    if (((u32)idx >= PARAM_TOTAL) || (addr == NULL) || (size <= 0))
    {
        LOG_E("param write fail. input parameter error.");
        return(-RT_ERROR);
    }
    
    ptype = param_msg_table[idx].type;
    psize = param_msg_table[idx].size;
    paddr = param_datas + param_offset_table[idx];
    
    param_mutex_take();
    
    switch (ptype)
    {
    case PTYPE_STR:
        psize = strlen((const char *)paddr);
        if (psize > (size - 1))
        {
            psize = (size - 1);
        }
        memcpy(addr, paddr, psize);
        ((u8*)addr)[psize] = 0;
        break;
    case PTYPE_ARRAY:
        if (psize > size)
        {
            psize = size;
        }
        memcpy(addr, paddr, psize);
        break;
    case PTYPE_INT:
        {
            s64 lval;
            
            if (size != sizeof(u8) && size != sizeof(u16) && size != sizeof(u32) && size != sizeof(u64))
            {
                psize = 0;
                break;
            }
            
            if (psize == sizeof(u32))
            {
                s32 tv;
                memcpy((u8 *)&tv, paddr, psize);
                lval = (s64)tv;
            }
            else
            {
                memcpy((u8 *)&lval, paddr, psize);
            }
            
            memcpy(addr, (u8 *)&lval, size);
            psize = size;
        }
        break;
    case PTYPE_HEX:
        {
            u64 lval;
            
            if (size != sizeof(u8) && size != sizeof(u16) && size != sizeof(u32) && size != sizeof(u64))
            {
                psize = 0;
                break;
            }
            
            if (psize == sizeof(u32))
            {
                u32 tv;
                memcpy((u8 *)&tv, paddr, psize);
                lval = (u64)tv;
            }
            else
            {
                memcpy((u8 *)&lval, paddr, psize);
            }
            
            memcpy(addr, (u8 *)&lval, size);
            psize = size;
        }
        break;
    case PTYPE_FLOAT:
        {
            f64 fval;

            if (size != sizeof(f32) && size != sizeof(f64))
            {
                psize = 0;
                break;
            }
            
            if (psize == sizeof(f32))
            {
                f32 tv;
                memcpy((u8*)&tv, paddr, psize);
                fval = (f64)tv;
            }
            else
            {
                memcpy((u8*)&fval, paddr, psize);
            }

            if (size == sizeof(f32))
            {
                f32 tv = fval;
                memcpy(addr, (u8 *)&tv, size);
            }
            else
            {
                memcpy(addr, (u8 *)&fval, size);
            }

            psize = size;
        }
        break;
    default:
        psize = 0;
        break;
    }

    param_mutex_release();
    
    return ((psize > 0) ? RT_EOK : -RT_ERROR);
}

int param_write_by_index(int idx, const void *addr, int size)
{
    param_type_t ptype;
    u32 psize;
    u8 *paddr;
    
    if (param_datas == NULL || param_mutex == NULL)
    {
        LOG_E("param write fail. param no initialized.");
        return(-RT_ERROR);
    }
    
    if (((u32)idx >= PARAM_TOTAL) || (addr == NULL) || (size <= 0))
    {
        LOG_E("param write fail. input parameter error.");
        return(-RT_ERROR);
    }
    
    ptype = param_msg_table[idx].type;
    psize = param_msg_table[idx].size;
    paddr = param_datas + param_offset_table[idx];
    
    param_mutex_take();
    
    switch (ptype)
    {
    case PTYPE_STR:
        size = strlen(addr);
        if (size > (psize - 1))
        {
            size = (psize - 1);
        }
        memcpy(paddr, addr, size);
        paddr[size] = 0;
        break;
    case PTYPE_ARRAY:
        if (size > psize)
        {
            size = psize;
        }
        memcpy(paddr, addr, size);
        break;
    case PTYPE_INT:
        {
            s64 lval;
            
            if (size != sizeof(u8) && size != sizeof(u16) && size != sizeof(u32) && size != sizeof(u64))
            {
                size = 0;
                break;
            }
            
            if (size == sizeof(u8))
            {
                lval = *((s8 *)addr);
            }
            else if (size == sizeof(u16))
            {
                s16 tv;
                memcpy((u8 *)&tv, addr, size);
                lval = (s64)tv;
            }
            else if (size == sizeof(u32))
            {
                s32 tv;
                memcpy((u8 *)&tv, addr, size);
                lval = (s64)tv;
            }
            else
            {
                memcpy((u8 *)&lval, addr, size);
            }
            
            memcpy(paddr, (u8 *)&lval, psize);
            size = psize;
        }
        break;
    case PTYPE_HEX:
        {
            u64 lval;
            
            if (size != sizeof(u8) && size != sizeof(u16) && size != sizeof(u32) && size != sizeof(u64))
            {
                size = 0;
                break;
            }
            
            if (size == sizeof(u8))
            {
                lval = *((u8 *)addr);
            }
            else if (size == sizeof(u16))
            {
                u16 tv;
                memcpy((u8 *)&tv, addr, size);
                lval = (u64)tv;
            }
            else if (size == sizeof(u32))
            {
                u32 tv;
                memcpy((u8 *)&tv, addr, size);
                lval = (u64)tv;
            }
            else
            {
                memcpy((u8 *)&lval, addr, size);
            }
            
            memcpy(paddr, (u8 *)&lval, psize);
            size = psize;
        }
        break;
    case PTYPE_FLOAT:
        {
            f64 fval;
            
            if (size != sizeof(f32) && size != sizeof(f64))
            {
                size = 0;
                break;
            }
            
            if (size == sizeof(f32))
            {
                f32 tv;
                memcpy((u8*)&tv, addr, size);
                fval = (f64)tv;
            }
            else
            {
                memcpy((u8*)&fval, addr, size);
            }

            if (psize == sizeof(f32))
            {
                f32 tv = fval;
                memcpy(paddr, (u8 *)&tv, psize);
            }
            else
            {
                memcpy(paddr, (u8 *)&fval, psize);
            }

            size = psize;
        }
        break;
    default:
        size = 0;
        break;
    }
    
    param_mutex_release();
    
    #ifdef PARAM_USING_AUTO_SAVE
    if (psize > 0)
    {
        param_auto_save_start();
    }
    #endif
    
    return ((psize > 0) ? RT_EOK : -RT_ERROR);
}

int param_resume_by_name(char *name)//resume default by name
{
    int idx = param_find_by_name(name);
    if (idx < 0)
    {
        LOG_E("param resume fail by name. parameter don`t exist.");
        return(-RT_ERROR);
    }
    
    return(param_resume_by_index(idx));
}

int param_read_by_name(char *name, void *addr, int size)//read param by name
{
    int idx = param_find_by_name(name);
    if (idx < 0)
    {
        LOG_E("param read fail by name. parameter don`t exist.");
        return(-RT_ERROR);
    }
    
    return(param_read_by_index(idx, addr, size));
}

int param_write_by_name(char *name, const void *addr, int size)//write param by name
{
    int idx = param_find_by_name(name);
    if (idx < 0)
    {
        LOG_E("param write fail by name. parameter don`t exist.");
        return(-RT_ERROR);
    }
    
    return(param_write_by_index(idx, addr, size));
}

#ifdef PARAM_USING_CLI
static void param_print_str(const char *str, int min_len)
{
    int empty_len = min_len - strlen(str);
    if (empty_len < 0)
    {
        empty_len = 0;
    }
    PARAM_PRINT("%s", str);
    for (int i=0; i<empty_len; i++)
    {
        PARAM_PRINT(" ");
    }
}

static void param_print_size(int size, int min_len)
{
    char str[8];
    sprintf(str, "%d", size);
    param_print_str(str, min_len);
}

static void param_print_type(int type, int size, int min_len)
{
    char *pstr;
    char str[8];
    switch(type)
    {
    case PTYPE_STR:
        pstr = "string";
        break;
    case PTYPE_ARRAY:
        pstr = "array";
        break;
    case PTYPE_INT:
        sprintf(str, "int%d", size*8);
        pstr = str;
        break;
    case PTYPE_HEX:
        sprintf(str, "hex%d", size*8);
        pstr = str;
        break;
    case PTYPE_FLOAT:
        if(size == sizeof(f32))
        {
            pstr = "float";
        }
        else
        {
            pstr = "double";
        }
        break;
    default:
        pstr = "unknow";
        break;
    }
    param_print_str(pstr, min_len);
}

static void param_print_value(int type, int size, void *val)
{
    switch(type)
    {
    case PTYPE_STR:
        PARAM_PRINT((char*)val);
        break;
    case PTYPE_ARRAY:
        for (int i = 0; i < size; i++)
        {
            u8 *pv = (u8*)val;
            if (i != 0)
            {   
                PARAM_PRINT(" ");
            }
            PARAM_PRINT("%02X", pv[i]);
        }
        break;
    case PTYPE_INT:
        if (size == sizeof(s8))
        {
            PARAM_PRINT("%d", *((s8*)val));
        }
        else if (size == sizeof(u16))
        {
            s16 lval;
            memcpy((u8 *)&lval, val, size);
            PARAM_PRINT("%d", lval);
        }
        else if (size == sizeof(u32))
        {
            s32 lval;
            memcpy((u8 *)&lval, val, size);
            PARAM_PRINT("%d", lval);
        }
        else if (size == sizeof(u64))
        {
            s64 lval;
            s32 tv;
            memcpy((u8 *)&lval, val, size);
            tv = lval / 1000000000;
            if (tv != 0)
            {
                PARAM_PRINT("%d", tv);
            }
            tv = lval % 1000000000;
            PARAM_PRINT("%d", tv);
        }
        break;
    case PTYPE_HEX:
        if (size == sizeof(u8))
        {
            PARAM_PRINT("%02X", *((u8*)val));
        }
        else if (size == sizeof(u16))
        {
            u16 lval;
            memcpy((u8 *)&lval, val, size);
            PARAM_PRINT("%04X", lval);
        }
        else if (size == sizeof(u32))
        {
            u32 lval;
            memcpy((u8 *)&lval, val, size);
            PARAM_PRINT("%08X", lval);
        }
        else if (size == sizeof(u64))
        {
            u64 lval;
            memcpy((u8 *)&lval, val, size);
            PARAM_PRINT("%08X", (u32)(lval >> 32));
            PARAM_PRINT("%08X", (u32)lval);
        }
        break;
    case PTYPE_FLOAT:
        if (size == sizeof(f32))
        {
            char tbuf[20];
            float fval = *((float*)val);
            sprintf(tbuf, "%.3f", fval);
            PARAM_PRINT(tbuf);
        }
        else
        {
            char tbuf[20];
            double fval = *((double*)val);
            sprintf(tbuf, "%.5f", fval);
            PARAM_PRINT(tbuf);
        }
        break;
    default:
        break;
    }
}

static void param_cmd(int argc, char **argv)
{
    if (argc < 2)
    {
        PARAM_PRINT("Usage: \n");
        PARAM_PRINT("param init              -Initialize parameter module.\n");
        PARAM_PRINT("param list              -List display all params.\n");
        PARAM_PRINT("param load              -Load all params from flash.\n");
        PARAM_PRINT("param save              -Save all params to flash.\n");
        PARAM_PRINT("param resume name       -Resume the param to default by name.\n");
        PARAM_PRINT("param read name         -Read the param by name.\n");
        PARAM_PRINT("param write name val    -Write the param by name.\n");
        PARAM_PRINT("\n");
        return ;
    }
    if (strcmp(argv[1], "init") == 0)
    {
        if (param_init() == RT_EOK)
        {
            PARAM_PRINT("param init success.\n");
        }
        return;
    }
    if (strcmp(argv[1], "deinit") == 0)
    {
        param_deinit();
        PARAM_PRINT("param deinit success.\n");
        return;
    }
    if (strcmp(argv[1], "list") == 0)
    {
        PARAM_PRINT("\n");
        PARAM_PRINT("name              type    size  value          \n");
        PARAM_PRINT("----------------  ------  ----  -------------  \n");
        for (int i=0; i<PARAM_TOTAL; i++)
        {
            char buf[128];
            char *name = (char *)param_get_name(i);
            int type = param_get_type(i);
            int size = param_get_size(i);
            param_print_str(name, 16+2);
            param_print_type(type, size, 6+2);
            param_print_size(size, 4+2);
            param_read_by_index(i, buf, size);
            param_print_value(type, size, buf);
            PARAM_PRINT("\n");
        }
        PARAM_PRINT("---- param total : %d ----", PARAM_TOTAL);
        PARAM_PRINT("\n");
        return;
    }
    if (strcmp(argv[1], "load") == 0)
    {
        if (param_load_from_flash() == RT_EOK)
        {
            PARAM_PRINT("param load success.\n");
        }
        return;
    }
    if (strcmp(argv[1], "save") == 0)
    {
        if (param_save_to_flash() == RT_EOK)
        {
            PARAM_PRINT("param save success.\n");
        }
        return;
    }
    if (strcmp(argv[1], "resume") == 0)
    {
        if (argc < 3)
        {
            PARAM_PRINT("param resume name       -Resume the params to default by name.\n");
        }
        else
        {
            if (strcmp(argv[2], "all") == 0)
            {
                if (param_resume_all() == RT_EOK)
                {
                    PARAM_PRINT("resume all param success.\n");
                }
            }
            else if (param_resume_by_name(argv[2]) == RT_EOK)
            {
                PARAM_PRINT("resume param success, the name is %s\n", argv[2]);
            }
        }
        return;
    }
    if (strcmp(argv[1], "read") == 0)
    {
        if (argc < 3)
        {
            PARAM_PRINT("param read name         -Read the param by name.\n");
        }
        else
        {
            char buf[128];
            int type, size;
            int idx = param_find_by_name(argv[2]);
            if (idx < 0)
            {
                PARAM_PRINT("this param don`t exist, the name is %s\n", argv[2]);
                return;
            }
            type = param_get_type(idx);
            size = param_get_size(idx);
            if (param_read_by_index(idx, buf, size) < 0)
            {
                PARAM_PRINT("read param error, the name is %s\n", argv[2]);
            }
            param_print_value(type, size, buf);
            PARAM_PRINT("\n");
        }
        return;
    }
    if (strcmp(argv[1], "write") == 0)
    {
        if (argc < 4)
        {
            PARAM_PRINT("param write name val    -Write the param by name.\n");
        }
        else
        {
            char buf[128];
            int type, size;
            int idx = param_find_by_name(argv[2]);
            if (idx < 0)
            {
                PARAM_PRINT("this param don`t exist, the name is %s\n", argv[2]);
                return;
            }
            type = param_get_type(idx);
            size = param_get_size(idx);
            param_input_value(buf, type, size, argv[3]);
            if (param_write_by_index(idx, buf, size) < 0)
            {
                PARAM_PRINT("write param error, the name is %s\n", argv[2]);
                return;
            }
            PARAM_PRINT("write param success, the name is %s\n", argv[2]);
        }
        return;
    }
    
    PARAM_PRINT("error! unsupported parameters.\n");
}
MSH_CMD_EXPORT_ALIAS(param_cmd, param, param module opration commands.);
#endif

#ifdef PARAM_USING_AUTO_INIT
static int param_auto_init(void)
{
    param_init();
    
    if (param_load_from_flash() != RT_EOK)
    {
        LOG_E("param automatic load fail from flash.");
        return(-RT_ERROR);
    }
    LOG_I("param automatic load success from flash.");

    return(RT_EOK);
}
INIT_ENV_EXPORT(param_auto_init);
#endif


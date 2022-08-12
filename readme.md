# qparam软件包

## 1.简介

**qparam** 是一个快捷实现参数固化、恢复、列表、修改、以及快速存取等功能的软件包。

### 1.1目录结构

`qparam` 软件包目录结构如下所示：

``` 
qparam
├───inc                     // 头文件目录
│   └───param.h             // API 接口头文件
├───src                     // 源码目录
│   └───param.c             // 源代码文件
├───port                    // 接口目录
│   |   param_def.h         // 参数定义头文件
│   └───param_index.h       // 参数索引定义头文件
│   license                 // 软件包许可证
│   readme.md               // 软件包使用说明
└───SConscript              // RT-Thread 默认的构建脚本
```

### 1.2许可证

qparam package 遵循 LGPLv2.1 许可，详见 `LICENSE` 文件。

### 1.3依赖

- RT_Thread 4.0
- fal
- crclib

## 2.使用

### 2.1接口函数说明

#### int param_init(void);
- 功能 ：参数初始化
- 参数 ：无
- 返回 ：0--成功, <0--失败

#### void param_deinit(void);
- 功能 ：参数去初始化
- 参数 ：无
- 返回 ：无

#### int param_load_from_flash(void);
- 功能 ：从flash装载参数
- 参数 ：无
- 返回 ：0--成功, <0--失败

#### int param_save_to_flash(void);
- 功能 ：保存参数到flash
- 参数 ：无
- 返回 ：0--成功, <0--失败

#### int param_resume_all(void);
- 功能 ：恢复全部参数到默认值
- 参数 ：无
- 返回 ：0--成功, <0--失败

#### int param_resume_by_name(char *name);
- 功能 ：恢复指定名称的参数到默认值
- 参数 ：name--参数名称
- 返回 ：0--成功, <0--失败

#### int param_read_by_name(char *name, void *addr, int size);
- 功能 ：通过参数名读取参数值
- 参数 ：name--参数名称
- 参数 ：addr--保存参数值的变量指针
- 参数 ：addr--保存参数值的变量尺寸
- 返回 ：0--成功, <0--失败

#### int param_write_by_name(char *name, const void *addr, int size);
- 功能 ：通过参数名修改参数值
- 参数 ：name--参数名称
- 参数 ：addr--保存参数值的变量指针
- 参数 ：addr--保存参数值的变量尺寸
- 返回 ：0--成功, <0--失败

#### const char *param_get_name(int idx);;
- 功能 ：获取指定索引的参数名称
- 参数 ：idx--参数索引
- 返回 ：参数名指针, NULL--表示参数不存在

#### int param_resume_by_index(int idx);
- 功能 ：恢复指定索引的参数到默认值
- 参数 ：idx--参数索引
- 返回 ：0--成功, <0--失败

#### int param_read_by_index(int idx, void *addr, int size);
- 功能 ：通过索引读取参数值
- 参数 ：idx--参数索引
- 参数 ：addr--保存参数值的变量指针
- 参数 ：addr--保存参数值的变量尺寸
- 返回 ：0--成功, <0--失败

#### int param_write_by_index(int idx, const void *addr, int size);
- 功能 ：通过索引修改参数值
- 参数 ：idx--参数索引
- 参数 ：addr--保存参数值的变量指针
- 参数 ：addr--保存参数值的变量尺寸
- 返回 ：0--成功, <0--失败

### 2.3获取组件

- **方式1：**
通过 *Env配置工具* 或 *RT-Thread studio* 开启软件包，根据需要配置各项参数；配置路径为 *RT-Thread online packages -> miscellaneous packages -> quick param* 


### 2.4配置参数说明

| 参数宏 | 说明 |
| ---- | ---- |
| PARAM_USING_INDEX         | 使用通过索引快速存取参数功能
| PARAM_USING_CLI           | 使用通过命令行列表、读取、修改参数功能
| PARAM_USING_AUTO_INIT     | 使用自动初始化参数功能
| PARAM_USING_AUTO_SAVE     | 使用自动保存参数功能
| PARAM_AUTO_SAVE_DELAY     | 自动保存参数的延时时间
| PARAM_PART_NAME           | 保存参数的fal分区名
| PARAM_SECTOR_SIZE         | 保存参数的flash扇区尺寸
| PARAM_SAVE_ADDR           | 保存参数的偏移地址
| PARAM_SAVE_ADDR_BAK       | 保存备份参数的偏移地址

## 3. 联系方式

* 维护：qiyongzhong
* 主页：https://gitee.com/qiyongzhong0/rt-thread-qparam
* 邮箱：917768104@qq.com

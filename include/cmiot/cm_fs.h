/**
 * @file        cm_fs.h
 * @brief       文件系统通用API接口
 * @copyright   Copyright © 2021 China Mobile IOT. All rights reserved.
 * @author      By cmiot3000
 * @date        2021/4/7
 *
 * @defgroup fs_common common
 * @ingroup FS
 * @{
 */

#ifndef __CM_FS_H__
#define __CM_FS_H__

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define O_RDONLY (0)      /*!< 示例，实际对应关系由芯片平台文件系统决定 */
#define O_WRONLY (1)      /*!< 示例，实际对应关系由芯片平台文件系统决定 */
#define O_RDWR (2)        /*!< 示例，实际对应关系由芯片平台文件系统决定 */
#define O_APPEND _FAPPEND /*!< 示例，实际对应关系由芯片平台文件系统决定 */
#define O_CREAT _FCREAT   /*!< 示例，实际对应关系由芯片平台文件系统决定 */
#define O_TRUNC _FTRUNC   /*!< 示例，实际对应关系由芯片平台文件系统决定 */

#ifndef SEEK_SET
#define SEEK_SET (0) /*!< 示例，实际对应关系由芯片平台文件系统决定 */
#endif
#ifndef SEEK_CUR
#define SEEK_CUR (1) /*!< 示例，实际对应关系由芯片平台文件系统决定 */
#endif
#ifndef SEEK_END
#define SEEK_END (2) /*!< 示例，实际对应关系由芯片平台文件系统决定 */
#endif

/* 模组使用LittleFS文件系统，请按照下文描述的文件打开方式使用。错误使用时文件系统无法保证文件功能正常 */
#define CM_FS_RB            (0)      /*!< rb，打开一个二进制文件，文件必须存在，只允许读 */
#define CM_FS_WB            (1)      /*!< wb，新建一个二进制文件，已存在的文件内容清空，只允许写 */
#define CM_FS_AB            (2)      /*!< ab，打开或新建一个二进制文件，只允许在文件末尾追写 */
#define CM_FS_WBPLUS        (3)      /*!< wb+，新建一个二进制文件，已存在的文件内容清空，允许读写 */
#define CM_FS_ABPLUS        (4)      /*!< ab+，打开或新建一个二进制文件，可读，只允许在文件末尾追写 */
#define CM_FS_RBPLUS        (5)      /*!< rb+，打开一个二进制文件，文件必须存在，允许读写 */

#define CM_FS_SEEK_SET      (0)      /*!< SEEK_SET，文件开头 */
#define CM_FS_SEEK_CUR      (1)      /*!< SEEK_CUR，当前位置 */
#define CM_FS_SEEK_END      (2)      /*!< SEEK_END，文件结尾 */

#define CM_FS_MAX_PATH (255) /*!< 示例，实际大小由芯片平台文件系统决定 */

/****************************************************************************
 * Public Types
 ****************************************************************************/
typedef struct
{
    uint32_t free_size;  /*!< 当前可用文件系统大小 */
    uint32_t total_size; /*!< 文件系统总大小 */
    uint32_t extfs_free_size;  /*!< 外挂flash可用文件系统大小 */    
    uint32_t extfs_total_size; /*!< 外挂flash文件系统总大小 */
} cm_fs_system_info_t;

typedef struct
{
    uint32_t file_attr;                /*!< 0：（相对）路径        1：文件 */
    uint32_t file_size;                /*!< file_attr = 1时为文件大小，否则该参数无效 */
    uint8_t file_name[CM_FS_MAX_PATH+1]; /*!< file_attr = 1时为文件名称，  file_attr = 0时为（相对）路径名称 */
} cm_fs_file_data_t;

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C" {
#else
#define EXTERN extern
#endif

/****************************************************************************/

 /**
 *  @brief 打开一个文件
 *
 *  @param [in] filename 文件路径
 *  @param [in] flag 打开参数
 *
 *  @return
 *   >= 0 - 文件描述符 \n
 *   <  0 - 错误
 * 
 *  @details ml307y：写模式下，父目录不存在时支持自动创建父目录
 */
int32_t cm_fs_open(const char *filename, int32_t flag);

/**
 *  @brief 关闭一个文件
 *
 *  @param [in] fd 文件描述符
 *
 *  @return
 *   = 0 - 成功 \n
 *   < 0 - 失败
 */
int32_t cm_fs_close(int32_t fd);

/**
 *  @brief 读取文件内容
 *
 *  @param [in]  fd   文件描述符
 *  @param [out] buf  存储数据的缓冲区指针
 *  @param [in]  size 要读取的数据长度
 *
 *  @return
 *   >= 0 - 实际的读取长度 \n
 *   <  0 - 读取失败
 */
int32_t cm_fs_read(int32_t fd, void *buf, uint32_t size);

/**
 *  @brief 写入文件内容
 *
 *  @param [in] fd   文件描述符
 *  @param [in] buf  存储数据的缓冲区指针
 *  @param [in] size 要写入的数据长度
 *
 *  @return
 *   >= 0 - 实际的写入长度 \n
 *   <  0 - 写入失败
 */
int32_t cm_fs_write(int32_t fd, const void *buf, uint32_t size);

/**
 *  @brief 将处于堆栈状态的文件与存储设备中的文件同步
 *
 *  @param [in] fd 文件描述符
 *
 *  @return
 *   = 0 - 同步成功 \n
 *   < 0 - 同步失败
 */
int32_t cm_fs_sync(int32_t fd);

/**
 *  @brief 文件指针定位
 *
 *  @param [in] fd 文件描述符
 *  @param [in] offset 指针偏移量
 *  @param [in] base 偏移起始点，SEEK_SET：文件开头 SEEK_CUR：当前位置 SEEK_END：文件末尾
 *
 *  @return
 *   = 0 - 成功 \n
 *   < 0 - 失败
 */
int32_t cm_fs_seek(int32_t fd, int32_t offset, int32_t base);

/**
 *  @brief 删除文件
 *
 *  @param [in] file_name 文件路径
 *
 *  @return
 *   >= 0 - 成功 \n
 *   <  0 - 失败
 */
int32_t cm_fs_delete(const char *file_name);

/**
 *  @brief 移动文件或更改文件名
 *
 *  @param [in] src  源路径
 *  @param [in] dest 目的路径
 *
 *  @return
 *   = 0 - 成功 \n
 *   < 0 - 失败
 *
 *  @details 当前后文件路径不同时，为移动文件，当前后路径相同时，为更改文件名
 */
int32_t cm_fs_move(const char *src, const char *dest);

/**
 *  @brief 检查文件/文件夹目录是否存在
 *
 *  @param [in] path 文件路径
 *
 *  @return
 *   = 1 - 存在 \n
 *   = 0 - 不存在
 */
int32_t cm_fs_exist(const char *path);

/**
 *  @brief 获取文件大小
 *
 *  @param [in] file_name 文件路径
 *  @return
 *
 *   >= 0 - 文件长度 \n
 *   <  0 - 操作失败
 *
 *  @details More details
 */
int32_t cm_fs_filesize(const char *file_name);

/**
 *  @brief 创建文件夹
 *
 *  @param [in] path 文件夹路径
 *  @return
 *   = 0 - 成功 \n
 *   < 0 - 失败
 *
 *  @details 支持多级目录创建
 */
int32_t cm_fs_mkdir(const char *path);

/**
 *  @brief 删除文件夹
 *
 *  @param [in] path 文件夹路径
 *  @return
 *   = 0 - 成功 \n
 *   < 0 - 失败
 *
 *  @details 只有当目录为空目录时，才应删除该目录
 */
int32_t cm_fs_rmdir(const char *path);

/**
 *  @brief 获取文件系统信息
 *
 *  @param [out] info 文件系统信息
 *
 *  @return
 *   = 0 - 成功 \n
 *   < 0 - 失败
 */
int32_t cm_fs_getinfo(cm_fs_system_info_t *info);

/**
 *  @brief 打开查找，并且获取文件夹下第一个文件或文件夹目录信息
 *  @param [in]  path      文件夹路径
 *  @param [out] file_data 对应路径下的第一个文件或（相对）路径信息
 *
 *  @return
 *   非 0 - 查找返回的句柄 \n
 *   <  0 - 查找失败
 */
int32_t cm_fs_find_first(const char *path, cm_fs_file_data_t *file_data);

/**
 *  @brief 获取文件夹中下一个文件信息
 *  @param [in]  find_fd   查找句柄（cm_fs_find_first接口返回值）
 *  @param [out] file_data 对应路径下的下一个文件或文件夹目录信息
 *
 *  @return
 *   = 0  - 成功 \n
 *   <  0 - 失败
 *
 *  @details More details
 */
int32_t cm_fs_find_next(int32_t find_fd, cm_fs_file_data_t *file_data);

/**
 *  @brief 关闭查找
 *
 *  @param [in]  find_fd      查找句柄（cm_fs_find_first接口返回值）
 *
 *  @return
 *   = 0 - 成功 \n
 *   < 0 - 失败
 *
 *  @details More details
 */
int32_t cm_fs_find_close(int32_t find_fd);

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __CM_FS_H__ */

/** @}*/

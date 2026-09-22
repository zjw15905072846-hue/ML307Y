/*********************************************************
 *  @file    cm_demo_sd.h
 *  @brief   OpenCPU SD header
 *  Copyright (c) 2025 China Mobile IOT.
 *  All rights reserved.
 *  created by cmiot 2025/06/20
 ********************************************************/
#include "stdio.h"
#include "stdlib.h"
#include "cm_sd.h"
#include "cm_demo_uart.h"
#include "cm_demo_sd.h"

/**
 *  UART口SD功能调试使用示例
 *  sd ismount               //查询查询（SD卡）文件系统是否挂载
 *  sd getinfo               //获取文件系统信息
 *  sd write                 //write功能测试
 *  sd read                  //read功能测试
 *  sd seek                  //seek功能测试
 *  sd move                  //move功能测试
 *  sd delete                //delete功能测试
 */
void cm_test_sd(EmbeddedCli *cli, char *args, void *context)
{
    const char *cmd = embeddedCliGetToken(args, 1);
    if (cmd == NULL)
    {
        cm_demo_printf("invalid param\n");
        return;
    }

    int ret = 0;

    if (0 == strcmp((const char *)cmd, "ismount"))
    {
        ret = cm_sd_is_mount();
        cm_demo_printf("[SD] cm_sd_is_mount() ret:%d\n", ret);
    }
    else if (0 == strcmp((const char *)cmd, "getinfo"))
    {
        cm_sd_system_info_t info = {};
        ret = cm_sd_getinfo(&info);
        cm_demo_printf("[SD] cm_sd_getinfo() ret:%d total:%llu free:%llu\n", ret, info.total_size, info.free_size);
    }
    else if (0 == strcmp((const char *)cmd, "write"))
    {
        ret = cm_sd_mkdir("/TEST1");
        cm_demo_printf("[SD] cm_sd_mkdir() ret:%d\n", ret);
        ret = cm_sd_set_currentdir("/TEST1");
        cm_demo_printf("[SD] cm_sd_set_currentdir() ret:%d\n", ret);

        char currentdir[256] = {};

        ret = cm_sd_get_currentdir(currentdir, 256);
        cm_demo_printf("[SD] cm_sd_get_currentdir() ret:%d %s\n", ret, currentdir);

        int32_t fd = cm_sd_fopen("/TEST1/test1.c", "wb");
        cm_demo_printf("[SD] cm_sd_fopen() ret:%d\n", fd);
        ret = cm_sd_fwrite(fd, "123456780", strlen("123456780"));
        cm_demo_printf("[SD] cm_sd_write() ret:%d\n", ret);
        ret = cm_sd_fclose(fd);
        cm_demo_printf("[SD] cm_sd_fclose() ret:%d\n", ret);
    }
    else if (0 == strcmp((const char *)cmd, "read"))
    {
        if (true == cm_sd_exist("/TEST1/test1.c"))
        {
            cm_demo_printf("[SD] test1.c\n");
            int32_t filesize = cm_sd_filesize("/TEST1/test1.c");
            cm_demo_printf("[SD] cm_sd_filesize() ret:%d\n", filesize);

            if (0 < filesize)
            {
                char buff[20] = {};
                int32_t fd = cm_sd_fopen("/TEST1/test1.c", "r");
                cm_demo_printf("[SD] cm_sd_fopen() ret:%d\n", fd);
                cm_sd_fread(fd, buff, filesize);
                cm_demo_printf("[SD] cm_sd_fread() ret:%d, buff is %s\n", ret, buff);
                ret = cm_sd_fclose(fd);
                cm_demo_printf("[SD] cm_sd_fclose() ret:%d\n", ret);
            }
        }
        else if (true == cm_sd_exist("/TEST1/test2.c"))
        {
            cm_demo_printf("[SD] test2.c\n");
            int32_t filesize = cm_sd_filesize("/TEST1/test2.c");
            cm_demo_printf("[SD] cm_sd_filesize() ret:%d\n", filesize);

            if (0 < filesize)
            {
                char buff[20] = {};
                int32_t fd = cm_sd_fopen("/TEST1/test2.c", "r");
                cm_demo_printf("[SD] cm_sd_fopen() ret:%d\n", fd);
                cm_sd_fread(fd, buff, filesize);
                cm_demo_printf("[SD] cm_sd_fread() ret:%d, buff is %s\n", ret, buff);
                ret = cm_sd_fclose(fd);
                cm_demo_printf("[SD] cm_sd_fclose() ret:%d\n", ret);
            }
        }
        else
        {
            cm_demo_printf("[SD] no file\n");
        }
    }
    else if (0 == strcmp((const char *)cmd, "seek"))
    {
        if (true == cm_sd_exist("/TEST1/test1.c"))
        {
            cm_demo_printf("[SD] test1.c\n");
            int32_t fd = cm_sd_fopen("/TEST1/test1.c", "w");
            cm_demo_printf("[SD] cm_sd_fopen() ret:%d\n", fd);
            ret = cm_sd_fseek(fd, 1, CM_SD_SEEK_SET);
            ret = cm_sd_fwrite(fd, "ABC", strlen("ABC"));
            cm_demo_printf("[SD] cm_sd_write() ret:%d\n", ret);
            ret = cm_sd_fseek(fd, 2, CM_SD_SEEK_CUR);
            ret = cm_sd_fwrite(fd, "DE", strlen("DE"));
            cm_demo_printf("[SD] cm_sd_write() ret:%d\n", ret);
            ret = cm_sd_fseek(fd, 0, CM_SD_SEEK_END);
            ret = cm_sd_fwrite(fd, "F", strlen("F"));
            cm_demo_printf("[SD] cm_sd_write() ret:%d\n", ret);
            ret = cm_sd_fclose(fd);
            cm_demo_printf("[SD] cm_sd_fclose() ret:%d\n", ret);
        }
        else if (true == cm_sd_exist("/TEST1/test2.c"))
        {
            cm_demo_printf("[SD] test2.c\n");
            
        }
        else
        {
            cm_demo_printf("[SD] no file\n");
        }
    }
    else if (0 == strcmp((const char *)cmd, "move"))
    {
        ret = cm_sd_fmove("/TEST1/test1.c", "/TEST1/test2.c");
        cm_demo_printf("[SD] cm_sd_move() ret:%d\n", ret);
    }
    else if (0 == strcmp((const char *)cmd, "delete"))
    {
        char currentdir[256] = {};

        if (true == cm_sd_exist("/TEST1/test1.c"))
        {
            cm_demo_printf("[SD] test1.c\n");
            ret = cm_sd_fdelete("/TEST1/test1.c");
            cm_demo_printf("[SD] cm_sd_delete() ret:%d\n", ret);
            ret = cm_sd_mkdir("/other");
            cm_demo_printf("[SD] cm_sd_mkdir() ret:%d\n", ret);
            ret = cm_sd_set_currentdir("/other");
            cm_demo_printf("[SD] cm_sd_set_currentdir() ret:%d\n", ret);
            ret = cm_sd_get_currentdir(currentdir, 256);
            cm_demo_printf("[SD] cm_sd_get_currentdir() ret:%d %s\n", ret, currentdir);
            ret = cm_sd_rmdir("/TEST1");
            cm_demo_printf("[SD] cm_sd_rmdir() ret:%d\n", ret);
        }
        else if (true == cm_sd_exist("/TEST1/test2.c"))
        {
            cm_demo_printf("[SD] test2.c\n");
            ret = cm_sd_fdelete("/TEST1/test2.c");
            cm_demo_printf("[SD] cm_sd_delete() ret:%d\n", ret);
            ret = cm_sd_mkdir("/other");
            cm_demo_printf("[SD] cm_sd_mkdir() ret:%d\n", ret);
            ret = cm_sd_set_currentdir("/other");
            cm_demo_printf("[SD] cm_sd_set_currentdir() ret:%d\n", ret);
            ret = cm_sd_get_currentdir(currentdir, 256);
            cm_demo_printf("[SD] cm_sd_get_currentdir() ret:%d %s\n", ret, currentdir);
            ret = cm_sd_rmdir("/TEST1");
            cm_demo_printf("[SD] cm_sd_rmdir() ret:%d\n", ret);
        }
        else
        {
            cm_demo_printf("[SD] no file\n");
        }
    }
    else if (0 == strcmp((const char *)cmd, "speed_test1"))
    {
        ret = cm_sd_mkdir("/TEST1");
        cm_demo_printf("[SD] cm_sd_mkdir() ret:%d\n", ret);
        ret = cm_sd_set_currentdir("/TEST1");
        cm_demo_printf("[SD] cm_sd_set_currentdir() ret:%d\n", ret);

        char currentdir[256] = {};

        ret = cm_sd_get_currentdir(currentdir, 256);
        cm_demo_printf("[SD] cm_sd_get_currentdir() ret:%d %s\n", ret, currentdir);

        int32_t fd = cm_sd_fopen("/TEST1/test.c", "wb");
        cm_demo_printf("[SD] cm_sd_fopen() ret:%d\n", fd);

        char *tmp = malloc(1024*10);
        int i = 0;
        for(i = 0; i<10240; i++)
        {
            ret = cm_sd_fwrite(fd, tmp, 1024*10);
            if((i%20 == 0) || (ret <=0))
            {
               // cm_demo_printf("[SD] cm_sd_write() ret:%d %d\n", i,ret);
            }
        }
        free(tmp);
        cm_demo_printf("[SD] cm_sd_write() ret:%d %d\n", i,ret);
        ret = cm_sd_fclose(fd);
        cm_demo_printf("[SD] cm_sd_fclose() ret:%d\n", ret);
    }
    else if (0 == strcmp((const char *)cmd, "speed_test2"))
    {
        if (true == cm_sd_exist("/TEST1/test.c"))
        {
            cm_demo_printf("[SD] test.c\n");
            int32_t filesize = cm_sd_filesize("/TEST1/test.c");
            cm_demo_printf("[SD] cm_sd_filesize() ret:%d\n", filesize);

            if (0 < filesize)
            {
                char *tmp = malloc(1024*10);
                int32_t fd = cm_sd_fopen("/TEST1/test.c", "r");
                cm_demo_printf("[SD] cm_sd_fopen() ret:%d\n", fd);

                int i = 0;
                for(i = 0; i<10240; i++)
                {
                    ret = cm_sd_fread(fd, tmp, 1024*10);
                    if((i%20 == 0) || (ret <=0))
                    {
                        //cm_demo_printf("[SD] cm_sd_read() ret:%d %d\n", i,ret);
                    }
                    if(ret <= 0) break; 
                }
                free(tmp);
                cm_demo_printf("[SD] cm_sd_read() ret:%d %d\n", i,ret);
                ret = cm_sd_fclose(fd);
                cm_demo_printf("[SD] cm_sd_fclose() ret:%d\n", ret);
            }
        }
        else
        {
            cm_demo_printf("[SD] no file\n");
        }
    }
    else
    {
        cm_demo_printf("[SD] Illegal operation\n"); 
    }
    return;
}

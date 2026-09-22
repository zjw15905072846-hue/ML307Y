/**
 * @file        cm_iomux.h
 * @brief       IOMUX接口
 * @copyright   Copyright @2021 China Mobile IOT. All rights reserved.
 * @author      By cmiot1325
 * @date        2021/05/18
 *
 * @defgroup iomux
 * @ingroup PI
 * @{
 */
/**********************************************************************************
 ***********************IOMUX使用注意事项*************************************
 * 1、设备上电之后需要将各个引脚功能进行IOMUX配置,以唯一功能进行调试，中途不可变更;
 ***************************************************************************/

#ifndef __CM_IOMUX_H__
#define __CM_IOMUX_H__

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
/****************************************************************************
 * Public Types
 ****************************************************************************/
/**IOMUX PIN  definition 和模组实际PIN脚对应一致,请参照资源综述*/
typedef enum{
    CM_IOMUX_PIN_0 = 0,    
    CM_IOMUX_PIN_1,        
    CM_IOMUX_PIN_2,        
    CM_IOMUX_PIN_3,        
    CM_IOMUX_PIN_4,        
    CM_IOMUX_PIN_5,        
    CM_IOMUX_PIN_6,        
    CM_IOMUX_PIN_7,        
    CM_IOMUX_PIN_8,        
    CM_IOMUX_PIN_9,        
    CM_IOMUX_PIN_10,       
    CM_IOMUX_PIN_11,       
    CM_IOMUX_PIN_12,       
    CM_IOMUX_PIN_13,       
    CM_IOMUX_PIN_14,       
    CM_IOMUX_PIN_15,       
    CM_IOMUX_PIN_16,       
    CM_IOMUX_PIN_17,       
    CM_IOMUX_PIN_18,       
    CM_IOMUX_PIN_19,       
    CM_IOMUX_PIN_20,       
    CM_IOMUX_PIN_21,       
    CM_IOMUX_PIN_22,       
    CM_IOMUX_PIN_23,       
    CM_IOMUX_PIN_24,       
    CM_IOMUX_PIN_25,       
    CM_IOMUX_PIN_26,       
    CM_IOMUX_PIN_27,       
    CM_IOMUX_PIN_28,       
    CM_IOMUX_PIN_29,       
    CM_IOMUX_PIN_30,       
    CM_IOMUX_PIN_31,       
    CM_IOMUX_PIN_32,       
    CM_IOMUX_PIN_33,       
    CM_IOMUX_PIN_34,       
    CM_IOMUX_PIN_35,       
    CM_IOMUX_PIN_36,       
    CM_IOMUX_PIN_37,       
    CM_IOMUX_PIN_38,       
    CM_IOMUX_PIN_39,       
    CM_IOMUX_PIN_40,       
    CM_IOMUX_PIN_41,       
    CM_IOMUX_PIN_42,       
    CM_IOMUX_PIN_43,       
    CM_IOMUX_PIN_44,       
    CM_IOMUX_PIN_45,       
    CM_IOMUX_PIN_46,       
    CM_IOMUX_PIN_47,       
    CM_IOMUX_PIN_48,       
    CM_IOMUX_PIN_49,       
    CM_IOMUX_PIN_50,       
    CM_IOMUX_PIN_51,       
    CM_IOMUX_PIN_52,       
    CM_IOMUX_PIN_53,       
    CM_IOMUX_PIN_54,       
    CM_IOMUX_PIN_55,       
    CM_IOMUX_PIN_56,       
    CM_IOMUX_PIN_57,       
    CM_IOMUX_PIN_58,       
    CM_IOMUX_PIN_59,       
    CM_IOMUX_PIN_60,       
    CM_IOMUX_PIN_61,       
    CM_IOMUX_PIN_62,       
    CM_IOMUX_PIN_63,       
    CM_IOMUX_PIN_64,       
    CM_IOMUX_PIN_65,       
    CM_IOMUX_PIN_66,       
    CM_IOMUX_PIN_67,       
    CM_IOMUX_PIN_68,       
    CM_IOMUX_PIN_69,       
    CM_IOMUX_PIN_70,       
    CM_IOMUX_PIN_71,       
    CM_IOMUX_PIN_72,       
    CM_IOMUX_PIN_73,       
    CM_IOMUX_PIN_74,       
    CM_IOMUX_PIN_75,       
    CM_IOMUX_PIN_76,       
    CM_IOMUX_PIN_77,       
    CM_IOMUX_PIN_78,       
    CM_IOMUX_PIN_79,       
    CM_IOMUX_PIN_80,       
    CM_IOMUX_PIN_81,       
    CM_IOMUX_PIN_82,       
    CM_IOMUX_PIN_83,       
    CM_IOMUX_PIN_84,       
    CM_IOMUX_PIN_85,       
    CM_IOMUX_PIN_86,       
    CM_IOMUX_PIN_87,
    CM_IOMUX_PIN_88,    
    CM_IOMUX_PIN_89,
    CM_IOMUX_PIN_90,
    CM_IOMUX_PIN_91,
    CM_IOMUX_PIN_92,
    CM_IOMUX_PIN_93,
    CM_IOMUX_PIN_94,
    CM_IOMUX_PIN_95,
    CM_IOMUX_PIN_96,
    CM_IOMUX_PIN_97,
    CM_IOMUX_PIN_98,    
    CM_IOMUX_PIN_99,
    CM_IOMUX_PIN_100,
    CM_IOMUX_PIN_101,
    CM_IOMUX_PIN_102,
    CM_IOMUX_PIN_103,
    CM_IOMUX_PIN_104,
    CM_IOMUX_PIN_105,
    CM_IOMUX_PIN_106,
    CM_IOMUX_PIN_107,
    CM_IOMUX_PIN_108,
    CM_IOMUX_PIN_109,
    CM_IOMUX_PIN_MAX      
} cm_iomux_pin_e;

/*IOMUX FUNC  definition */
typedef enum{
    CM_IOMUX_FUNC_FUNCTION0 ,         /*!<不支持*/
    CM_IOMUX_FUNC_FUNCTION1,          /*!<功能1*/
    CM_IOMUX_FUNC_FUNCTION2,          /*!<功能2*/
    CM_IOMUX_FUNC_FUNCTION3,          /*!<功能3*/
    CM_IOMUX_FUNC_FUNCTIONNUM_END,
} cm_iomux_func_e;

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
/*IOMUX PIN FUNCTION  definition*/


/****************************************************************************
 * Public Data
 ****************************************************************************/


/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/
#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/**
 * @brief IOMUX 设置引脚复用功能
 *
 * @param [in] pin PIN 定义号
 * @param [in] fun FUN 定义号
 *
 *  @return  
 *    = 0  - 成功 \n
 *    = -1 - 失败
 */
int32_t cm_iomux_set_pin_func(cm_iomux_pin_e pin, cm_iomux_func_e fun);

/**
 * @brief IOMUX 获取引脚功能
 *
 * @param [in] pin PIN 定义号
 * @param [out] fun FUN 定义号
 *
 * @return  
 *    = 0  - 成功\n
 *    < 0  - 失败
 */
int32_t cm_iomux_get_pin_func(cm_iomux_pin_e pin, cm_iomux_func_e *fun);
 
#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __CM_IOMUX_H__ */

/** @}*/

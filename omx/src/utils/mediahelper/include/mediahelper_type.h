/*
 **                Copyright 2013, MARVELL SEMICONDUCTOR, LTD.
 ** THIS CODE CONTAINS CONFIDENTIAL INFORMATION OF MARVELL.
 ** NO RIGHTS ARE GRANTED HEREIN UNDER ANY PATENT, MASK WORK RIGHT OR COPYRIGHT
 ** OF MARVELL OR ANY THIRD PARTY. MARVELL RESERVES THE RIGHT AT ITS SOLE
 ** DISCRETION TO REQUEST THAT THIS CODE BE IMMEDIATELY RETURNED TO MARVELL.
 ** THIS CODE IS PROVIDED "AS IS". MARVELL MAKES NO WARRANTIES, EXPRESSED,
 ** IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY, COMPLETENESS OR PERFORMANCE.
 **
 ** MARVELL COMPRISES MARVELL TECHNOLOGY GROUP LTD. (MTGL) AND ITS SUBSIDIARIES,
 ** MARVELL INTERNATIONAL LTD. (MIL), MARVELL TECHNOLOGY, INC. (MTI), MARVELL
 ** SEMICONDUCTOR, INC. (MSI), MARVELL ASIA PTE LTD. (MAPL), MARVELL JAPAN K.K.
 ** (MJKK), MARVELL ISRAEL LTD. (MSIL).
 */
#ifndef MEDIA_HELPER_TYPE_H_
#define MEDIA_HELPER_TYPE_H_

/** MEDIA_U8 is an 8 bit unsigned quantity that is byte aligned */
typedef unsigned char MEDIA_U8;

/** MEDIA_S8 is an 8 bit signed quantity that is byte aligned */
typedef signed char MEDIA_S8;

/** MEDIA_U16 is a 16 bit unsigned quantity that is 16 bit word aligned */
typedef unsigned short MEDIA_U16;

/** MEDIA_S16 is a 16 bit signed quantity that is 16 bit word aligned */
typedef signed short MEDIA_S16;

/** MEDIA_U32 is a 32 bit unsigned quantity that is 32 bit word aligned */
typedef unsigned long MEDIA_U32;

/** MEDIA_S32 is a 32 bit signed quantity that is 32 bit word aligned */
typedef signed long MEDIA_S32;

/** The MEDIA_BOOL type is intended to be used to represent a true or a false
    value
 */
typedef enum MEDIA_BOOL {
    MEDIA_FALSE = 0,
    MEDIA_TRUE = !MEDIA_FALSE,
    MEDIA_BOOL_MAX = 0x7FFFFFFF
} MEDIA_BOOL;


#endif //MEDIA_HELPER_TYPE_H_

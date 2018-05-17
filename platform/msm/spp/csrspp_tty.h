/*
 * QTIL Synergy for Linux SPP TTY Driver.
 *
 * Copyright (c) 2014-2016 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>

#ifndef CSRSPPDRV_H

#define CSRSPPDRV_H

typedef unsigned long int CsrUint32;
typedef signed   long   int     CsrInt32;
typedef signed long int Csrint32;
typedef unsigned short int CsrUint16;
typedef signed short int Csrint16;
typedef unsigned char CsrUint8;
typedef signed  char Csrint8;

#define SPP_RELEASE_ONHUP  0
#define SPP_HANGUP_NOW     1
#define SPP_TTY_ATTACHED   2
#define SPP_TTY_RELEASED   3
#define SPP_TTY_LINK_LOS   4

#define SPP_TTY_MTU       1500

#define SPP_CTL_DATA_LEN   9
#define SPP_CTL_OP_POS     0
#define SPP_CTL_INS_POS    1
#define SPP_CTL_MTU_POS    5

#define SPP_CTL_ON         1
#define SPP_CTL_OFF        2

#define NL_MSG_BASE      0x00
#define NL_MSG_CTL       NL_MSG_BASE

#define SPP_SKB_HEAD_RESERVE	8
#define SPP_SKB_TAIL_RESERVE	2
#define SPP_SKB_RESERVE  (SPP_SKB_HEAD_RESERVE + SPP_SKB_TAIL_RESERVE)

#define SPP_AVAILABLE 0
#define SPP_OCCUPY 1

#define NETLINK_SOCKET_SPP         31
#define SPP_INSTANCE_SUM           4

#define TRUE             (1)
#define FALSE            (0)

#define SPPCREATEDEV    _IOW('R', 200, int)

#define RFCOMM_MAX_DEV  16

#endif /* CSRSPPDRV */

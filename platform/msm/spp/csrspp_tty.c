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

#include <linux/capability.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/socket.h>
#include <linux/netlink.h>
#include <linux/security.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#include <net/net_namespace.h>
#include <net/netns/generic.h>

#include "csrspp_tty.h"

#define SPP_TTY_PORTS    10
#define SPP_TTY_MAJOR    16
#define SPP_TTY_MINOR    0

#define BUFFER_32        32

#define CSR_DEBUG
/* Debug macros*/
#if defined(CSR_DEBUG)  /* limited debug messages */
#define CSRSPP_DBG(fmt, arg...)  printk(KERN_INFO "(csrspp_drv):"fmt"\n" , ## arg)
#define CSRSPP_VER(fmt, arg...)  printk(KERN_INFO "(csrspp_drv):"fmt"\n" , ## arg)
#define CSRSPP_ERR(fmt, arg...)  printk(KERN_ERR "(csrspp_drv):"fmt"\n" , ## arg)

#elif defined(VERBOSE) /* Print all messages */
#define CSRSPP_DBG(fmt, arg...)(fmt, arg...) printk(KERN_INFO "(csrspp_drv):"fmt"\n" , ## arg)
#define CSRSPP_VER(fmt, arg...) printk(KERN_INFO "(csrspp_drv):"fmt"\n" , ## arg)
#define CSRSPP_ERR(fmt, arg...) printk(KERN_ERR "(csrspp_drv):"fmt"\n" , ## arg)

#else /* Error Messages only */
#define CSRSPP_ERR(fmt, arg...) printk(KERN_ERR "(csrspp_drv):"fmt"\n" , ## arg)
#define CSRSPP_VER(fmt, arg...)
#define CSRSPP_DBG(fmt, arg...)
#endif

static struct tty_driver *spp_tty_driver;
static int mDevId[RFCOMM_MAX_DEV + 1];
static struct sock *control_sock;

#define spp_dlc_lock(d)     spin_lock(&d->lock)
#define spp_dlc_unlock(d)   spin_unlock(&d->lock)
#define RCV_SKB_FAIL(err) do { netlink_ack(skb, nlh, (err)); return; } while (0)

struct spp_dev
{
    struct list_head    list;
    int                 openFlag;
    int                 id;
    struct tty_port     port;
    unsigned long       flags;
    char                name[12];
    int                 sppInstance; /* SPP instance id returned by native SPP module */
    CsrUint16           profileMaxFrameSize;
    int                 socket_pid;
    bool                ttyReady;
    struct task_struct  *read_task;
    spinlock_t          lock;
    struct tty_struct       *tty;
    struct device           *tty_dev;
};

static LIST_HEAD(spp_dev_list);
static DEFINE_RWLOCK(spp_dev_lock);
static DEFINE_MUTEX(spp_dev_mutex);
static DEFINE_MUTEX(spp_writing_mutex);
static DEFINE_MUTEX(spp_netlink_mutex);
static DEFINE_MUTEX(spp_send_off_mutex);

static inline struct spp_dev *spp_dev_get(int sppInstance);
static int spp_create_dev(int sppInstanceId);
static int spp_release_dev(int sppInstance);
static int spp_enable_dev(int sppInstanceId, int profileMaxFrameSize, int pid);
static int spp_disable_dev(int sppInstanceId);
static int spp_tty_read(struct spp_dev *dev, unsigned int dataLen, unsigned char *data);
static void spp_dev_destruct(struct tty_port *port);
static void spp_tty_cleanup(struct tty_struct *tty);

static int spp_dev_activate(struct tty_port *port, struct tty_struct *tty)
{
    struct spp_dev *dev = NULL;

    dev = container_of(port, struct spp_dev, port);
    if (dev == NULL)
    {
        CSRSPP_ERR("%s dev is NULL", __func__);
        return -1;
    }

    CSRSPP_DBG("spp_dev_activate, get the dev %p", dev);
    spp_dlc_lock(dev);
    dev->tty = tty;
    dev->tty->driver_data = dev;
    spp_dlc_unlock(dev);

    return 0;
}

static void spp_dev_shutdown(struct tty_port *port)
{
    struct spp_dev *dev = NULL;

    dev = container_of(port, struct spp_dev, port);
    if (dev == NULL)
    {
        CSRSPP_ERR("%s dev is NULL", __func__);
        return;
    }

    CSRSPP_DBG("spp_dev_shutdown, get the dev %p", dev);
}

static void spp_dev_destruct(struct tty_port *port)
{
    struct spp_dev *dev = NULL;

    dev = container_of(port, struct spp_dev, port);
    if (dev == NULL)
    {
        CSRSPP_ERR("%s dev is NULL", __func__);
        return;
    }

    CSRSPP_DBG("%s dev id %d, dev->name: %s, dev->tty_dev: %p openFlag: %d", __func__, 
                dev->id, dev->name, dev->tty_dev, dev->openFlag);

    if (dev->openFlag == 1 && dev->tty->driver_data != NULL) /* release when open serial */
        dev->tty->driver_data = NULL;

    clear_bit(SPP_RELEASE_ONHUP, &dev->flags);

    if (dev->tty_dev)
        tty_unregister_device(spp_tty_driver, dev->id);

    mutex_lock(&spp_dev_mutex);
    mDevId[dev->id - 1] = SPP_AVAILABLE;
    list_del(&dev->list);
    mutex_unlock(&spp_dev_mutex);

    kfree(dev);

    /* It's safe to call module_put() here because socket still
           holds reference to this module. */
    module_put(THIS_MODULE);
}

static const struct tty_port_operations csrspp_port_ops = {
    .destruct = spp_dev_destruct,
    .activate = spp_dev_activate,
    .shutdown = spp_dev_shutdown,
};

static int spp_disable_dev(int sppInstanceId)
{
    struct spp_dev *dev = NULL;

    dev = spp_dev_get(sppInstanceId);
    if (!dev)
    {
        CSRSPP_ERR("%s dev is NULL", __func__);
        return -1;
    }

    dev->ttyReady = FALSE;

    return 0;
}

static int spp_release_dev(int sppInstance)
{
    struct spp_dev *dev = NULL;
    struct tty_struct *tty = NULL;
    struct tty_port *port = NULL;

    dev = spp_dev_get(sppInstance);
    if (dev == NULL)
    {
        CSRSPP_ERR("%s get dev failed", __func__);
        return -ENODEV;
    }

    /* CSRSPP_DBG("spp_release_dev,dev->id %d flags 0x%x", dev->id, dev->flags); */

    if (!test_bit(SPP_RELEASE_ONHUP, &dev->flags))
    {
        CSRSPP_ERR("%s dev already released", __func__);
        tty_port_put(&dev->port);
        return -ENODEV;
    }

    port = &dev->port;
    port->itty = NULL; /* tty_port_destructor wil check last port ref was dropped before tty release */

    tty_port_put(port);
    /* Shut down TTY synchronously before freeing spp_dev */
    tty = tty_port_tty_get(port);
    CSRSPP_DBG("tty_port_tty_get, tty %p", tty);
    if (tty)
    {
        CSRSPP_DBG("%s, tty->index is %d", __func__, tty->index);
        tty_vhangup(tty);
        tty_kref_put(tty);
        tty->driver_data = NULL;
        tty->driver->ttys[tty->index] = NULL;  /* update tty state , after release do close tty_release_checks will return -1 */
    }

    return 0;
}

static struct spp_dev *__spp_dev_get(int sppInstance)
{
    struct spp_dev *dev = NULL;
    struct list_head  *p;

    list_for_each(p, &spp_dev_list)
    {
        dev = list_entry(p, struct spp_dev, list);
        CSRSPP_DBG("__spp_dev_get, dev_id %d sppInstance 0x%x", dev->id,
                   dev->sppInstance);

        if (dev->sppInstance == sppInstance)
            return dev;
    }

    return NULL;
}

static inline struct spp_dev *spp_dev_get(int sppInstance)
{
    struct spp_dev *dev = NULL;

    read_lock(&spp_dev_lock);

    dev = __spp_dev_get(sppInstance);

    if (dev)
    {
        if (test_bit(SPP_TTY_RELEASED, &dev->flags))
        {
            dev = NULL;
            CSRSPP_ERR("SPP dev has been released");
        }
    }

    read_unlock(&spp_dev_lock);

    return dev;
}

static struct spp_dev *__spp_dev_get_by_id(int id)
{
    struct spp_dev *dev = NULL;
    struct list_head  *p;

    list_for_each(p, &spp_dev_list)
    {
        dev = list_entry(p, struct spp_dev, list);

        if (dev->id == id)
            return dev;
    }

    return NULL;
}

static inline struct spp_dev *spp_dev_get_by_id(int id)
{
    struct spp_dev *dev = NULL;

    read_lock(&spp_dev_lock);

    dev = __spp_dev_get_by_id(id);
    if (dev != NULL)
    {
        if (test_bit(SPP_TTY_RELEASED, &dev->flags))
            dev = NULL;
    }

    read_unlock(&spp_dev_lock);

    return dev;
}

static int spp_tty_send_message(CsrInt32 sppInstance, unsigned char *buf, int count)
{
    struct spp_dev *dev = NULL;
    struct sk_buff *skb = NULL;
    int err = 0, sent = 0, size = 0;
    int mtu = 0;

    dev = spp_dev_get(sppInstance);
    if (dev == NULL)
    {
        CSRSPP_ERR("%s get dev fail", __func__);
        return -ENODEV;
    }

    mtu = dev->profileMaxFrameSize;

    CSRSPP_DBG("%s: spp_tty_send_message: buf %s, count %d", __func__, buf, count);

    while (count)
    {
        size = min_t(uint, count, mtu);

        skb = alloc_skb(size + SPP_SKB_RESERVE, GFP_ATOMIC);
        if (!skb)
            break;

        skb_reserve(skb, SPP_SKB_HEAD_RESERVE);
        memcpy(skb_put(skb, size), buf + sent, size);

        err = netlink_unicast(control_sock, skb, dev->socket_pid, 0);

        if (err < 0)
        {
            CSRSPP_ERR("%s netlink_unicast send dev id error", __func__);
            kfree_skb(skb);
            skb = NULL;
            break;
        }

        sent  += size;
        count -= size;
    }
    /* CSRSPP_DBG("%s: sent: %d, err: %d\n", __func__, sent, err); */

    return sent ? sent : err;
}
static int spp_enable_dev(int sppInstanceId, int profileMaxFrameSize, int pid)
{
    struct spp_dev *dev = NULL;

    dev = spp_dev_get(sppInstanceId);
    if (!dev)
    {
        CSRSPP_ERR("%s dev is NULL", __func__);
        return -ENODEV;
    }

    dev->profileMaxFrameSize = profileMaxFrameSize;
    dev->socket_pid = pid;
    dev->ttyReady = TRUE;

    return dev->id;
}

static inline void __spp_rcv_skb(struct sk_buff *skb)
{
    CsrUint8 *buffer = NULL;
    CsrInt32 retVal;
    CsrInt32 sppInstance;
    CsrInt32 profileMaxFrameSize;
    CsrUint8 mOper;
    int  nlmsglen, skblen, pid = 0;
    struct nlmsghdr *nlh;
    CsrUint8 dev_id[BUFFER_32];
    CsrUint8 *message = "Read_Thread_Exit";

    skblen = skb->len;
    nlh = nlmsg_hdr(skb);
    nlmsglen = nlh->nlmsg_len;

    /* CSRSPP_DBG("__spp_rcv_skb ,skblen is %d,nlmsglen is %d,data len is %d! \n", 
                skblen, nlmsglen, nlmsglen - NLMSG_LENGTH(0)); */

    if (nlmsglen < sizeof(*nlh) || skblen < nlmsglen)
    {
        CSRSPP_ERR("%s nlmsglen or skblen error", __func__);
        return;
    }

    pid = NETLINK_CB(skb).portid;

    buffer = NLMSG_DATA(nlh);

    if (nlh->nlmsg_flags == NL_MSG_CTL)
    {
        mOper = buffer[SPP_CTL_OP_POS];

        switch (mOper)
        {
            case SPP_CTL_ON:
                memcpy(&sppInstance, buffer + SPP_CTL_INS_POS, sizeof(CsrInt32));
                memcpy(&profileMaxFrameSize, buffer + SPP_CTL_MTU_POS, sizeof(CsrInt32));
                CSRSPP_DBG("SPP ctrol on socket sppInstance is (%ld(b),profileMaxFrameSize is %ld(m)",
                            sppInstance, profileMaxFrameSize);
                retVal = spp_enable_dev(sppInstance, profileMaxFrameSize, pid);
                CSRSPP_DBG("spp_create_dev ret is %ld", retVal);

                snprintf(dev_id, sizeof(dev_id), "ttySpp%ld", retVal);

                mutex_lock(&spp_writing_mutex);
                CSRSPP_DBG("ON, enter writing mutex");
                spp_tty_send_message(sppInstance, dev_id, strlen(dev_id));
                mutex_unlock(&spp_writing_mutex);
                CSRSPP_DBG("ON, leave writing mutex");
                break;

            case SPP_CTL_OFF:
                memcpy(&sppInstance, buffer + SPP_CTL_INS_POS, sizeof(CsrInt32));
                CSRSPP_DBG("SPP ctrol off socket sppInstance is %ld!", sppInstance);
                mutex_lock(&spp_send_off_mutex);
                CSRSPP_DBG("OFF, enter spp_send_off_mutex");
                /* send exit message to spp read thread, due to netlink recv() funtion can't return*/
                retVal = spp_disable_dev(sppInstance);
                mutex_unlock(&spp_send_off_mutex);
                CSRSPP_DBG("OFF, leave spp_send_off_mutex");
                mutex_lock(&spp_writing_mutex);
                spp_tty_send_message(sppInstance, message, strlen(message));
                mutex_unlock(&spp_writing_mutex);
                break;

            default :
                CSRSPP_ERR("SPP ctrol mOper over the range is %d!", mOper);
                break;
        }
    }
    else
    {
        struct spp_dev *dev = NULL;
        /* CSRSPP_DBG("SPP data type is %d!", nlh->nlmsg_flags); */
        sppInstance = nlh->nlmsg_flags;

        dev = spp_dev_get(sppInstance);
        if (dev == NULL)
        {
            CSRSPP_ERR("SPP get dev failed");
            return;
        }

        spp_tty_read(dev, nlmsglen - NLMSG_LENGTH(0), buffer);
    }
}

static void spp_rcv_skb(struct sk_buff *skb)
{
    mutex_lock(&spp_netlink_mutex);
    CSRSPP_DBG("NETLINK, enter netlink mutex");
    __spp_rcv_skb(skb);
    mutex_unlock(&spp_netlink_mutex);
    CSRSPP_DBG("NETLINK, leave netlink mutex");
}

static int spp_create_ctl_sock(void)
{
    struct netlink_kernel_cfg nlcfg = {  
        .input = spp_rcv_skb,  
    };

    control_sock = netlink_kernel_create(&init_net, NETLINK_SOCKET_SPP, &nlcfg);

    CSRSPP_DBG("create control_sock netlink_kernel_create\n");

    if (control_sock == NULL)
    {
        CSRSPP_ERR("create control_sock failed, netlink_kernel_create\n");
        return -1;
    }

    return 0;
}

#if 0
static void spp_tty_wakeup(unsigned long arg)
{
    struct spp_dev *dev = (void *) arg;
    struct tty_struct *tty = dev->tty;

    if (!tty)
        return;

    CSRSPP_DBG("dev %p tty %p", dev, tty);
    tty_wakeup(tty);
}
#endif

static void init_spp_dev(void)
{
    int i;
    int retVal;

    for (i = 0; i < SPP_INSTANCE_SUM; i++)
    {
        retVal = spp_create_dev(i + 1);

        CSRSPP_DBG("%s, retVal: %d ", __FUNCTION__, retVal);
    }
}

static int spp_create_dev(int sppInstanceId)
{
    struct spp_dev *dev = NULL;
    struct list_head *head = &spp_dev_list;
    int err = 0;

    CSRSPP_DBG("sppInstanceId %d", sppInstanceId);

    dev = kzalloc(sizeof(struct spp_dev), GFP_KERNEL);
    if (dev == NULL)
    {
        CSRSPP_ERR("%s, malloc dev failed", __func__);
        return -ENOMEM;
    }

    mutex_lock(&spp_dev_mutex);

    dev->id = sppInstanceId;

    snprintf(dev->name, sizeof(dev->name), "ttySpp%d", dev->id);

    CSRSPP_DBG("dev->name: %s", dev->name);

    tty_port_init(&dev->port);

    dev->port.ops = &csrspp_port_ops;

    list_add(&dev->list, head);

    set_bit(SPP_RELEASE_ONHUP, &dev->flags);

    CSRSPP_DBG("flag has been set to %ld", dev->flags);

    mDevId[sppInstanceId] = SPP_OCCUPY;

    /* It's safe to call __module_get() here because socket already holds reference to this module. */
    __module_get(THIS_MODULE);

    mutex_unlock(&spp_dev_mutex);

    dev->tty_dev = tty_port_register_device(&dev->port, spp_tty_driver,
                                            dev->id, NULL);

    if (IS_ERR(dev->tty_dev))
    {
        tty_port_put(&dev->port);
        err = PTR_ERR(dev->tty_dev);
        list_del(&dev->list);
        goto free;
    }

    dev_set_drvdata(dev->tty_dev, dev);

    dev->sppInstance = sppInstanceId;
    dev->openFlag = 0;
    spin_lock_init(&dev->lock);

    return dev->id;

free:
    kfree(dev);
    return err;
}

static int spp_tty_open(struct tty_struct *tty, struct file *filp)
{
    struct spp_dev *dev = NULL;
    int err = 0;

    CSRSPP_DBG("spp_tty_open tty %p id %d", tty, tty->index);

    dev = (struct spp_dev *) tty->driver_data;

    if (dev == NULL)
    {
        CSRSPP_ERR("%s (%d) dev is NULL", __func__, __LINE__);

        return -1;
    }

    CSRSPP_DBG("%s dev %p opened: %d dev->name: %s id: %d", __func__,
                dev, dev->port.count, dev->name, dev->id);

    if (!dev->ttyReady)
    {
       CSRSPP_ERR("%s ttySPP%d not ready", __func__, dev->id);
       return -1;
    }

    if (dev->openFlag == 1)
    {
        CSRSPP_ERR("%s dev already opened", __func__);
        return 0;
    }
    err = tty_port_open(&dev->port, tty, filp);
    if (err)
    {
        CSRSPP_ERR("%s open failed, err: %d", __func__, err);
        return err;
    }

    dev->openFlag = 1;

    return 0;
}

static void spp_tty_close(struct tty_struct *tty, struct file *filp)
{
    struct spp_dev *dev = (struct spp_dev *) tty->driver_data;

    if (dev == NULL)
    {
        CSRSPP_ERR("%s get dev fail", __func__);
        return;
    }

    if (dev->openFlag != 1)
    {
        CSRSPP_ERR("%s, dev tty not opened, dev->openFlag: %d, dev->name %s", __func__, dev->openFlag, dev->name);
        CSRSPP_ERR("%s dev %p, id: %d, tty->driver_data: %p", __func__, dev, dev->id, tty->driver_data);
        return;
    }

    CSRSPP_DBG("tty %p dev %p opened %d", tty, dev, dev->port.count);
    CSRSPP_DBG("%s tty %p id %d", __func__, tty, tty->index);
    CSRSPP_DBG("%s dev %p, id: %d, tty->driver_data: %p", __func__, dev, dev->id, tty->driver_data);

    dev->openFlag = 0;
    tty_port_close(&dev->port, tty, filp);

    mutex_lock(&spp_dev_mutex);
    tty->driver_data = NULL;
    mutex_unlock(&spp_dev_mutex);
}

static int spp_tty_read(struct spp_dev *dev, unsigned int dataLen, unsigned char *data)
{
    int len = 0;
    struct tty_port *port = NULL;

    len = dataLen;
    port = &dev->port;

    if (port == NULL)
    {
        CSRSPP_ERR("%s, port is NULL", __func__);
        return -1;
    }

    if (!dev->ttyReady)
    {
       CSRSPP_ERR("%s ttySPP%d not ready", __func__, dev->id);
       return -1;
    }

    /* CSRSPP_DBG("%s, data: %s", __func__, data); */

    tty_insert_flip_string(port, data, len);
    tty_flip_buffer_push(port);

    return 0;
}

static int spp_tty_write(struct tty_struct *tty, const unsigned char *buf, int count)
{
    struct spp_dev *dev = (struct spp_dev *) tty->driver_data;
    struct sk_buff *skb = NULL;
    int err = 0, sent = 0, size = 0;
    int mtu = dev->profileMaxFrameSize;

    /* CSRSPP_DBG("%s: spp_tty_write: count %d", __func__, count); */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
    if (tty->flow.stopped)
#else
    if (tty->stopped)
#endif
        return 0;

    if (buf == NULL)
    {
        CSRSPP_ERR("%s buff is NULL", __func__);
        return err;
    }

    if (!dev->ttyReady)
    {
       CSRSPP_ERR("%s ttySPP%d not ready", __func__, dev->id);
       return err;
    }

    mutex_lock(&spp_writing_mutex);
    CSRSPP_DBG("%s: enter spp_writing_mutex", __func__);

    while (count)
    {
        size = min_t(uint, count, mtu);

        skb = alloc_skb(size + SPP_SKB_RESERVE, GFP_ATOMIC);
        if (skb == NULL)
        {
            CSRSPP_ERR("%s alloc skb failed", __func__);
            break;
        }

        skb_reserve(skb, SPP_SKB_HEAD_RESERVE);
        memcpy(skb_put(skb, size), buf + sent, size);

        err = netlink_unicast(control_sock, skb, dev->socket_pid, 0);
        if (err < 0)
        {
            CSRSPP_ERR("%s netlink_unicast data error", __func__);
            kfree_skb(skb);
            break;
        }

        sent  += size;
        count -= size;
    }

    mutex_unlock(&spp_writing_mutex);
    CSRSPP_DBG("%s: leave spp_writing_mutex", __func__);
    /* CSRSPP_DBG("%s: sent: %d, err: %d\n", __func__, sent, err); */

    return sent ? sent : err;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
static unsigned int spp_tty_write_room(struct tty_struct *tty)
#else
static int spp_tty_write_room(struct tty_struct *tty)
#endif
{
    return 0;
}

static void spp_tty_set_termios(struct tty_struct *tty, struct ktermios *old)
{
    /* struct ktermios *new = tty->termios;
    struct spp_dev *dev = (struct spp_dev *) tty->driver_data; */

    return;
}

static void spp_tty_throttle(struct tty_struct *tty)
{
    //struct spp_dev *dev = (struct spp_dev *) tty->driver_data;
    return;
}

static void spp_tty_unthrottle(struct tty_struct *tty)
{
    return;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
static unsigned int spp_tty_chars_in_buffer(struct tty_struct *tty)
#else
static int spp_tty_chars_in_buffer(struct tty_struct *tty)
#endif
{
    return 0;
}

static void spp_tty_flush_buffer(struct tty_struct *tty)
{
    /* struct spp_dev *dev = (struct spp_dev *) tty->driver_data; */

    return;
}

static void spp_tty_send_xchar(struct tty_struct *tty, char ch)
{
    return;
}

static void spp_tty_wait_until_sent(struct tty_struct *tty, int timeout)
{
    return;
}

static void spp_tty_hangup(struct tty_struct *tty)
{
    struct spp_dev *dev = NULL;

    dev = (struct spp_dev *) tty->driver_data;
    if (dev == NULL)
    {
        CSRSPP_ERR("%s dev is NULL", __func__);
        return;
    }

    CSRSPP_DBG("%s, tty %p dev %p, id %d", __func__, tty, dev, tty->index);

    tty_port_hangup(&dev->port);
}

static int spp_tty_tiocmget(struct tty_struct *tty)
{
    return 0;
}

static int spp_tty_tiocmset(struct tty_struct *tty, unsigned int set, unsigned int clear)
{
    return 0;
}

/* 
 *do the reverse of install, clearing the tty fields
 */
static void spp_tty_cleanup(struct tty_struct *tty)
{
//    struct spp_dev *dev = tty->driver_data;

    CSRSPP_DBG("%s, id %d", __func__, tty->index);

    mutex_lock(&spp_dev_mutex);
    tty->driver_data = NULL;
    mutex_unlock(&spp_dev_mutex);
}

static int spp_tty_install(struct tty_driver *driver, struct tty_struct *tty)
{
    struct spp_dev *dev = NULL;
    int err = 0;

    CSRSPP_DBG("%s, tty->index: %d", __func__, tty->index);
    dev = spp_dev_get(tty->index);
    if (!dev)
    {
        CSRSPP_ERR("%s dev is NULL", __func__);
        return -ENODEV;
    }

    if (!dev->ttyReady)
    {
       CSRSPP_ERR("%s ttySPP%d not ready", __func__, dev->id);
       return -ENODEV;
    }

    /* Attach TTY */
    mutex_lock(&spp_dev_mutex);
    tty->driver_data = dev;
    dev->tty = tty;
    mutex_unlock(&spp_dev_mutex);
    set_bit(SPP_TTY_ATTACHED, &dev->flags);

    /* install the tty_port */
    err = tty_port_install(&dev->port, driver, tty);
    if (err) {
        CSRSPP_ERR("Install the tty port failed");
        spp_tty_cleanup(tty);
        return err;
    }

    return 0;
}

/* ---- TTY structure ---- */
static const struct tty_operations spp_ops =
{
    .open           = spp_tty_open,
    .close          = spp_tty_close,
    .write          = spp_tty_write,
    .write_room     = spp_tty_write_room,
    .chars_in_buffer    = spp_tty_chars_in_buffer,
    .flush_buffer       = spp_tty_flush_buffer,
    .throttle       = spp_tty_throttle,
    .unthrottle     = spp_tty_unthrottle,
    .set_termios    = spp_tty_set_termios,
    .send_xchar     = spp_tty_send_xchar,
    .hangup         = spp_tty_hangup,
    .wait_until_sent    = spp_tty_wait_until_sent,
    .tiocmget       = spp_tty_tiocmget,
    .tiocmset       = spp_tty_tiocmset,
    .install        = spp_tty_install,
};

static int __init csrspp_init(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
    spp_tty_driver = tty_alloc_driver(SPP_TTY_PORTS, 0);
#else
    spp_tty_driver = alloc_tty_driver(SPP_TTY_PORTS);
#endif

    if (!spp_tty_driver)
        return -1;

    spp_tty_driver->num         = SPP_TTY_MAJOR;
    spp_tty_driver->owner       = THIS_MODULE;
    spp_tty_driver->driver_name = "ttySpp";
    spp_tty_driver->name        = "ttySpp";
    spp_tty_driver->major       = 0;
    spp_tty_driver->minor_start = 0;
    spp_tty_driver->type        = TTY_DRIVER_TYPE_SERIAL;
    spp_tty_driver->subtype     = SERIAL_TYPE_NORMAL;
    spp_tty_driver->flags       = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
    spp_tty_driver->init_termios    = tty_std_termios;
    spp_tty_driver->init_termios.c_cflag  = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
    spp_tty_driver->init_termios.c_lflag &= ~ICANON;
    spp_tty_driver->init_termios.c_cflag &= ~CSIZE;
    spp_tty_driver->init_termios.c_cflag &= ~CRTSCTS;
    spp_tty_driver->init_termios.c_cflag |= CS8;
    spp_tty_driver->init_termios.c_cflag &= ~CSTOPB;
    spp_tty_driver->init_termios.c_iflag |= IGNPAR;
    spp_tty_driver->init_termios.c_oflag = 0;
    spp_tty_driver->init_termios.c_lflag = 0;

    spp_tty_driver->init_termios.c_cc[VMIN] = 0;
    spp_tty_driver->init_termios.c_cc[VTIME] = 0;

    memset(mDevId, 0x00, RFCOMM_MAX_DEV);

    tty_set_operations(spp_tty_driver, &spp_ops);

    if (tty_register_driver(spp_tty_driver))
    {
        CSRSPP_ERR("%s: Can't register spp TTY driver", __func__);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
        tty_driver_kref_put(spp_tty_driver);
#else
        put_tty_driver(spp_tty_driver);
#endif
        return -1;
    }

    spp_create_ctl_sock();
    CSRSPP_DBG("spp_create_sock");

    init_spp_dev();

    printk("%s: SPP TTY layer initialized\n", __func__);

    return 0;
}

static void __exit csrspp_exit(void)
{
    int i;

    printk("spp_tty_exit\n");

    for (i = 0; i < SPP_INSTANCE_SUM; i++)
    {
        spp_release_dev(i + 1);
    }

    netlink_kernel_release(control_sock);
    control_sock = NULL;

    tty_unregister_driver(spp_tty_driver);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
    tty_driver_kref_put(spp_tty_driver);
#else
    put_tty_driver(spp_tty_driver);
#endif
}

module_init(csrspp_init);
module_exit(csrspp_exit);

MODULE_DESCRIPTION("QTIL Synergy for Linux SPP TTY Driver");
MODULE_LICENSE("GPL");

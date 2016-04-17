/* @file plustek-pp_sysdep.h
 * @brief a trial to centralize changes between the different 
 *        kernel-versions some stuff is maybe not relevant, but anyway...
 *
 * Copyright (C) 2000-2013 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * 0.30 - initial version
 * 0.38 - added this header
 * 0.39 - added kernel 2.4 stuff
 * 0.40 - added slab.h/malloc.h stuff for kernel >= 2.4.17
 * 0.41 - no changes
 * 0.42 - added _GET_TIME
 *      - added LINUX_26 for new kernel
 *      - added _MINOR
 * 0.43 - added class functions
 * 0.44 - added support for kernel >= 2.6.35 and 3.x
 * .
 * <hr>
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 *
 * As a special exception, the authors of SANE give permission for
 * additional uses of the libraries contained in this release of SANE.
 *
 * The exception is that, if you link a SANE library with other files
 * to produce an executable, this does not by itself cause the
 * resulting executable to be covered by the GNU General Public
 * License.  Your use of that executable is in no way restricted on
 * account of linking the SANE library code into it.
 *
 * This exception does not, however, invalidate any other reasons why
 * the executable file might be covered by the GNU General Public
 * License.
 *
 * If you submit changes to SANE to the maintainers to be included in
 * a subsequent release, you agree by submitting the changes that
 * those changes may be distributed with this exception intact.
 *
 * If you write modifications of your own for SANE, it is your choice
 * whether to permit this exception to apply to your modifications.
 * If you do not wish that, delete this exception notice.
 * <hr>
 */
#ifndef _SYSDEP_H_
#define _SYSDEP_H_

#ifndef LINUX_VERSION_CODE
# include <linux/version.h>
#endif

#ifndef VERSION_CODE
#  define VERSION_CODE(vers,rel,seq) ( ((vers)<<16) | ((rel)<<8) | (seq) )
#endif

/* only allow > 2.0.x */
#if LINUX_VERSION_CODE < VERSION_CODE(2,0,0)
#  error "This kernel is too old: not supported by this file"
#endif
#if LINUX_VERSION_CODE < VERSION_CODE(2,1,0)
#  define LINUX_20
#elif LINUX_VERSION_CODE < VERSION_CODE(2,4,0)
#  define LINUX_21
#elif LINUX_VERSION_CODE < VERSION_CODE(2,6,0)
#  define LINUX_24
#else
#  define LINUX_24
#  define LINUX_26
# include <linux/device.h>
# if LINUX_VERSION_CODE > VERSION_CODE(2,6,35)
#  define NOLOCK_IOCTL
#  define IOCTL unlocked_ioctl
# else
#  define IOCTL ioctl
# endif
# if LINUX_VERSION_CODE > VERSION_CODE(3,0,0)
#   include <linux/sched.h>
# endif
#endif

#include <linux/types.h> /* used later in this header */


/* Modularization issues */
#if LINUX_VERSION_CODE < VERSION_CODE(2,1,18)
#  define __USE_OLD_SYMTAB__
#  define EXPORT_NO_SYMBOLS register_symtab(NULL);
#  define REGISTER_SYMTAB(tab) register_symtab(tab)
#else
#  define REGISTER_SYMTAB(tab) /* nothing */
#endif

#ifdef __USE_OLD_SYMTAB__
#  define __MODULE_STRING(s)         /* nothing */
#  define MODULE_PARM(v,t)           /* nothing */
#  define MODULE_PARM_DESC(v,t)      /* nothing */
#  define MODULE_AUTHOR(n)           /* nothing */
#  define MODULE_DESCRIPTION(d)      /* nothing */
#  define MODULE_SUPPORTED_DEVICE(n) /* nothing */
#endif

#if LINUX_VERSION_CODE < VERSION_CODE(2,1,31)
# define CLOSETYPE     		void
# define CLOSERETURN(arg)
#else
# define CLOSETYPE     		int
# define CLOSERETURN(arg)	return arg
#endif

/*
 * "select" changed in 2.1.23. The implementation is twin, but this
 * header is new
 */
#if LINUX_VERSION_CODE > VERSION_CODE(2,1,22)
#  include <linux/poll.h>
#else
#  define __USE_OLD_SELECT__
#endif

/* Other change in the fops are solved using pseudo-types */
#if defined(LINUX_21) || defined(LINUX_24) || defined(LINUX_26)
#  define lseek_t      long long
#  define lseek_off_t  long long
#else
#  define lseek_t      int
#  define lseek_off_t  off_t
#endif

/* changed the prototype of read/write */
#if defined(LINUX_21) || defined (LINUX_24) || defined(LINUX_26) || defined(__alpha__)
# define count_t unsigned long
# define read_write_t long
#else
# define count_t int
# define read_write_t int
#endif


#if LINUX_VERSION_CODE < VERSION_CODE(2,1,31)
# define release_t void
#  define release_return(x) return
#else
#  define release_t int
#  define release_return(x) return (x)
#endif

/*
 * access to user space: use the 2.1 functions,
 * and implement them as macros for 2.0
 */

#ifdef LINUX_20
#  include <asm/segment.h>
#  define access_ok(t,a,sz)           (verify_area((t),(a),(sz)) ? 0 : 1)
#  define verify_area_20              verify_area
#  define copy_to_user(t,f,n)         (memcpy_tofs(t,f,n), 0)
#  define __copy_to_user(t,f,n)       copy_to_user((t),(f),(n))
#  define copy_to_user_ret(t,f,n,r)   copy_to_user((t),(f),(n))
#  define copy_from_user(t,f,n)       (memcpy_fromfs((t),(f),(n)), 0)
#  define __copy_from_user(t,f,n)     copy_from_user((t),(f),(n))
#  define copy_from_user_ret(t,f,n,r) copy_from_user((t),(f),(n))
#  define PUT_USER(val,add)           (put_user((val),(add)), 0)
#  define __PUT_USER(val,add)         PUT_USER((val),(add))
#  define PUT_USER_RET(val,add,ret)   PUT_USER((val),(add))
#  define GET_USER(dest,add)          ((dest)=get_user((add)), 0)
#  define __GET_USER(dest,add)        GET_USER((dest),(add))
#  define GET_USER_RET(dest,add,ret)  GET_USER((dest),(add))
#else
#  include <asm/uaccess.h>
#  include <asm/io.h>
#  define verify_area_20(t,a,sz) (0) /* == success */
#  define PUT_USER put_user
#  define __PUT_USER __put_user
#  define PUT_USER_RET put_user_ret
#  define GET_USER get_user
#  define __GET_USER __get_user
#  define GET_USER_RET get_user_ret

/* starting with 2.4.0-test8, they removed the put_user_ret and get_user_ret
 * macros, so we recode'em
 */
#if defined(LINUX_24) || defined(LINUX_26)
#ifndef put_user_ret
#  define put_user_ret(x,ptr,ret) ({ if (put_user(x,ptr)) return ret; })
#endif

#ifndef get_user_ret
#  define get_user_ret(x,ptr,ret) ({ if (get_user(x,ptr)) return ret; })
#endif
#endif

#endif

/* ioremap */
#ifdef LINUX_20
# define ioremap vremap
# define iounmap vfree
#endif

/* The use_count of exec_domain and binfmt changed in 2.1.23 */

#ifdef LINUX_20
#  define INCRCOUNT(p)  ((p)->module ? __MOD_INC_USE_COUNT((p)->module) : 0)
#  define CURRCOUNT(p)  ((p)->module && (p)->module->usecount)
#  define DECRCOUNT(p)  ((p)->module ? __MOD_DEC_USE_COUNT((p)->module) : 0)
#else
#  define INCRCOUNT(p)  ((p)->use_count++)
#  define CURRCOUNT(p)  ((p)->use_count)
#  define DECRCOUNT(p)  ((p)->use_count--)
#endif

/* register_dynamic no more existent -- just have 0 as inum */
#if LINUX_VERSION_CODE >= VERSION_CODE(2,1,29)
#  define proc_register_dynamic proc_register
#endif

#if LINUX_VERSION_CODE < VERSION_CODE(2,1,37)
#  define test_and_set_bit(nr,addr)  test_bit((nr),(addr))
#  define test_and_clear_bit(nr,addr) clear_bit((nr),(addr))
#  define test_and_change_bit(nr,addr) change_bit((nr),(addr))
#endif

/* 2.1.30 removed these functions. Let's define them, just in case */
#if LINUX_VERSION_CODE > VERSION_CODE(2,1,29)
#  define queue_task_irq      queue_task
#  define queue_task_irq_off  queue_task
#endif

/* 2.1.10 and 2.1.43 introduced new functions. They are worth using */

#if LINUX_VERSION_CODE < VERSION_CODE(2,1,10)

#  include <asm/byteorder.h>
#  ifdef __LITTLE_ENDIAN
#    define cpu_to_le16(x) (x)
#    define cpu_to_le32(x) (x)
#    define cpu_to_be16(x) htons((x))
#    define cpu_to_be32(x) htonl((x))
#  else
#    define cpu_to_be16(x) (x)
#    define cpu_to_be32(x) (x)
     extern inline __u16 cpu_to_le16(__u16 x) { return (x<<8) | (x>>8);}
     extern inline __u32 cpu_to_le32(__u32 x) { return((x>>24) |
             ((x>>8)&0xff00) | ((x<<8)&0xff0000) | (x<<24));}
#  endif

#  define le16_to_cpu(x)  cpu_to_le16(x)
#  define le32_to_cpu(x)  cpu_to_le32(x)
#  define be16_to_cpu(x)  cpu_to_be16(x)
#  define be32_to_cpu(x)  cpu_to_be32(x)

#endif

#if LINUX_VERSION_CODE < VERSION_CODE(2,1,43)
#  define cpu_to_le16p(addr) (cpu_to_le16(*(addr)))
#  define cpu_to_le32p(addr) (cpu_to_le32(*(addr)))
#  define cpu_to_be16p(addr) (cpu_to_be16(*(addr)))
#  define cpu_to_be32p(addr) (cpu_to_be32(*(addr)))

   extern inline void cpu_to_le16s(__u16 *a) {*a = cpu_to_le16(*a);}
   extern inline void cpu_to_le32s(__u16 *a) {*a = cpu_to_le32(*a);}
   extern inline void cpu_to_be16s(__u16 *a) {*a = cpu_to_be16(*a);}
   extern inline void cpu_to_be32s(__u16 *a) {*a = cpu_to_be32(*a);}

#  define le16_to_cpup(x) cpu_to_le16p(x)
#  define le32_to_cpup(x) cpu_to_le32p(x)
#  define be16_to_cpup(x) cpu_to_be16p(x)
#  define be32_to_cpup(x) cpu_to_be32p(x)

#  define le16_to_cpus(x) cpu_to_le16s(x)
#  define le32_to_cpus(x) cpu_to_le32s(x)
#  define be16_to_cpus(x) cpu_to_be16s(x)
#  define be32_to_cpus(x) cpu_to_be32s(x)
#endif

#if LINUX_VERSION_CODE < VERSION_CODE(2,1,15)
#  define __USE_OLD_REBUILD_HEADER__
#endif

#if LINUX_VERSION_CODE < VERSION_CODE(2,1,30)
#  define in_interrupt() (intr_count!=0)
#endif

/*
 * from 2.4.17 on, they decided to use slab.h instead of malloc.h... so what...
 * somewhere from 2.4.18-pre9 they skipped get_fast_time...
 */
#if LINUX_VERSION_CODE < VERSION_CODE(2,4,17)
#  include "linux/malloc.h"
#  define _GET_TIME get_fast_time
#else
#  include "linux/slab.h"
#  define _GET_TIME do_gettimeofday
#endif

#ifdef LINUX_26
#  define _MINOR(p) iminor(p)
#else
#  define _MINOR(p) minor(p->i_rdev)
#endif

/* Basic class macros */
#ifdef LINUX_26
#if LINUX_VERSION_CODE >= VERSION_CODE(2,6,15)

typedef struct class class_t;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#define CLASS_DEV_CREATE(class, devt, device, name) \
      device_create(class, device, devt, NULL, "%s", name)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
#define CLASS_DEV_CREATE(class, devt, device, name) \
        device_create(class, device, devt, name)
#else
#define CLASS_DEV_CREATE(class, devt, device, name) \
        class_device_create(class, NULL, devt, device, name)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
#define CLASS_DEV_DESTROY device_destroy
#else
#define CLASS_DEV_DESTROY class_device_destroy
#endif

#else /* LINUX 2.6.0 - 2.6.14 */

#if LINUX_VERSION_CODE >= VERSION_CODE(2,6,13) /* LINUX 2.6.13 - 2.6.14 */
typedef struct class class_t;
#define CLASS_DEVICE_CREATE            class_device_create
#define CLASS_DEV_DESTROY(class, devt) class_device_destroy(class, devt)

#else /* LINUX 2.6.0 - 2.6.12, class_simple */

typedef struct class_simple class_t;
#define CLASS_DEVICE_CREATE            class_simple_device_add
#define CLASS_DEV_DESTROY(class, devt) class_simple_device_remove(class, devt)

#define class_create class_simple_create
#define class_destroy class_simple_destroy
#define class_device_destroy(a, b) class_simple_device_remove(b)

#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,13) */
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15) */
#endif

#endif /* _SYSDEP_H_ */

/* END PLUSTEK-PP_SYSDEP.H ..................................................*/

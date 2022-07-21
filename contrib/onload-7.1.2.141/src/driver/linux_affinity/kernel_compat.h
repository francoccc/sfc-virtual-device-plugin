/* SPDX-License-Identifier: GPL-2.0 */
/* X-SPDX-Copyright-Text: (c) Solarflare Communications Inc */

#ifndef SFC_AFFINITY_KERNEL_COMPAT_H
#define SFC_AFFINITY_KERNEL_COMPAT_H

#include "driver/linux_affinity/autocompat.h"
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/net.h>
#include <linux/uaccess.h>
#include <asm/syscall.h>
#include <net/sock.h>

#if !defined(EFRM_HAVE_PROC_CREATE_DATA) && \
    !defined(EFRM_HAVE_PROC_CREATE_DATA_UMODE) && \
    !defined(EFRM_HAVE_PROC_CREATE_DATA_PROC_OPS)
#ifdef EFRM_HAVE_PROC_CREATE
static inline struct proc_dir_entry *
proc_create_data(const char *name, umode_t mode,
         struct proc_dir_entry *parent,
         const struct file_operations *proc_fops,
         void *data)
{
    struct proc_dir_entry *pde;
    pde = proc_create(name, mode, parent, proc_fops);
    if (pde != NULL)
        pde->data = data;
    return pde;
}
#else
static inline struct proc_dir_entry *
proc_create_data(const char *name, umode_t mode,
		 struct proc_dir_entry *parent,
		 const struct file_operations *proc_fops,
		 void *data)
{
	struct proc_dir_entry *pde;
	pde = create_proc_entry(name, mode, parent);
	if (pde != NULL) {
		pde->proc_fops = (struct file_operations *)proc_fops;
		pde->data = data;
	}
	return pde;
}
static inline struct proc_dir_entry *
proc_create(const char *name, umode_t mode,
		 struct proc_dir_entry *parent,
		 const struct file_operations *proc_fops)
{
	return proc_create_data(name, mode, parent, proc_fops, NULL);
}
#endif
#endif

#ifndef EFRM_HAVE_PDE_DATA
static inline void *PDE_DATA(const struct inode *inode)
{
	return PROC_I(inode)->pde->data;
}
#endif

#ifdef EFRM_OLD_DEV_BY_IDX
#define __dev_get_by_index(net_ns, ifindex) __dev_get_by_index(ifindex)
#define dev_get_by_index(net, ifindex) dev_get_by_index(ifindex)
#endif

#ifndef EFRM_HAVE_WAIT_QUEUE_ENTRY
#define wait_queue_entry_t wait_queue_t
#endif

#ifndef EFRM_HAVE_NEW_FAULT
typedef int vm_fault_t;
#endif



/* Correct sequence for per-cpu variable access is: disable preemption to
 * guarantee that the CPU is not changed under your feet - read/write the
 * variable - enable preemption.  In linux >=3.17, we have this_cpu_read()
 * which checks for preemption and get_cpu_var()/put_cpu_var() which
 * disable/enable preemption.
 *
 * We do not care about preemption at all, for 2 reasons:
 * 1. We do not really care if we sometimes get variable from wrong CPU.
 * 2. The most of uses are from driverlink, and NAPI thread can not
 *    change CPU.
 *
 * So, we use fast-and-unreliable raw_cpu_read().
 * For older kernels, we implement raw_cpu_read() and raw_cpu_write().
 */
#ifndef raw_cpu_read
/* linux < 3.17 */

#ifndef raw_cpu_ptr
/* linux < 3.15 */

#if defined(per_cpu_var) || LINUX_VERSION_CODE > KERNEL_VERSION(2,6,32)
/* per_cpu_var is defined from 2.6.30 to 2.6.33 */
#ifndef per_cpu_var
#define per_cpu_var(var) var
#endif

#define raw_cpu_ptr(var) \
      per_cpu_ptr(&per_cpu_var(var), raw_smp_processor_id())
#else
/* linux < 2.6.30 has per_cpu_ptr(), but it provides access to variables
 * allocated by alloc_percpu().  DEFINE_PER_CPU() defines another type of
 * variables, with per_cpu() and __raw_get_cpu_var() accessors. */
#define raw_cpu_ptr(var) (&__raw_get_cpu_var(var))
#endif

#endif /* raw_cpu_ptr */

#define raw_cpu_read(var) (*raw_cpu_ptr(var))
#define raw_cpu_write(var,val) \
  do {                          \
    *raw_cpu_ptr(var) = (val);  \
  } while(0)

#endif /* raw_cpu_read */

#ifndef EFRM_HAVE_FILE_INODE
/* Only RHEL6 doesn't have this function */
static inline struct inode *file_inode(const struct file *f)
{
        return f->f_path.dentry->d_inode;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
#define get_netns_id(net_ns)     ((net_ns)->ns.inum)
#elif defined(EFRM_NET_HAS_PROC_INUM)
#define get_netns_id(net_ns)     ((net_ns)->proc_inum)
#else
#define get_netns_id(net_ns)     0
#endif


/* Compat for linux < 5.5 */
#ifndef EFRM_HAVE_STRUCT_PROC_OPS
#define proc_ops file_operations
#define proc_open open
#define proc_read read
#define proc_write write
#define proc_lseek llseek
#define proc_release release
#define PROC_OPS_SET_OWNER .owner = THIS_MODULE,
#else
#define PROC_OPS_SET_OWNER
#endif

/* Compat for linux <= 3.16 */
#ifndef EFRM_HAS_STRUCT_TIMESPEC64
#define timespec64 timespec
#endif
#ifndef EFRM_HAS_KTIME_GET_TS64
#define ktime_get_ts64 ktime_get_ts
#endif

/* Linux < 5.8 does not have mmap_write_lock() */
#ifndef EFRM_HAVE_MMAP_LOCK_WRAPPERS
static inline void mmap_write_lock(struct mm_struct *mm)
{
  down_write(&mm->mmap_sem);
}
static inline void mmap_write_unlock(struct mm_struct *mm)
{
  up_write(&mm->mmap_sem);
}
static inline void mmap_read_lock(struct mm_struct *mm)
{
  down_read(&mm->mmap_sem);
}
static inline void mmap_read_unlock(struct mm_struct *mm)
{
  up_read(&mm->mmap_sem);
}
#endif

/* For linux<=5.7 you can use kernel_setsockopt(),
 * but newer versions doe not have this function. */
static inline int sock_ops_setsockopt(struct socket *sock,
                                      int level, int optname,
                                      char *optval, unsigned int optlen)
{
  int rc;
#ifndef EFRM_HAS_SOCKPTR
  mm_segment_t oldfs = get_fs();

  /* You should call sock_setsockopt() for SOL_SOCKET */
  WARN_ON(level == SOL_SOCKET);

  set_fs(KERNEL_DS);
  rc = sock->ops->setsockopt(sock, level, optname, optval, optlen);
  set_fs(oldfs);
#else
  rc = sock->ops->setsockopt(sock, level, optname,
                             KERNEL_SOCKPTR(optval), optlen);
#endif
  return rc;
}
#ifndef EFRM_HAS_SOCKPTR
#define USER_SOCKPTR(val) val
#endif

#endif

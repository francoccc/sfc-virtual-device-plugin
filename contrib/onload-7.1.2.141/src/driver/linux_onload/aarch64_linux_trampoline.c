/* SPDX-License-Identifier: GPL-2.0 */
/* X-SPDX-Copyright-Text: (c) Solarflare Communications Inc */

#include <ci/efrm/syscall.h>
#include "onload_kernel_compat.h"

#include <onload/linux_onload_internal.h>
#include <onload/linux_mmap.h>
#include <onload/linux_onload.h>
#include <asm/unistd.h>
#include <linux/unistd.h>
#include <asm/errno.h>
#include <asm/sysreg.h>
#include <asm/esr.h>
#include <asm/insn.h>
#include <asm/ptrace.h>

/*
 * This is somewhat dubious. On the one hand, the syscall
 * number is available to the syscall routine in x8 register,
 * so if it would ever like to consult it, it should be very much
 * upset by finding some random garbage there.
 * On another hand, in most contexts x8 is just a scratch register,
 * so the compiler theoretically could use it for its own purposes,
 * which the following code would mess up with. (Note that x8 is
 * _intentionally_ not marked as clobbered in the asm statement, so
 * that it would be forwarded to the original syscall).
 * However, in such short functions as our thunks, it seems unlikely and
 * empirically confirmed not to be.
 */
#define SET_SYSCALL_NO(_sc) \
  asm volatile("mov x8, %0" :: "i" (__NR_##_sc))

static asmlinkage typeof(sys_close) *saved_sys_close;

asmlinkage int efab_linux_sys_close(int fd)
{
  int rc;
  SET_SYSCALL_NO(close);
  rc = (int)saved_sys_close(fd);
  return rc;
}

#if CI_CFG_USERSPACE_EPOLL

static asmlinkage typeof(sys_epoll_create1) *saved_sys_epoll_create1;

int efab_linux_sys_epoll_create1(int flags)
{
  int rc;
  SET_SYSCALL_NO(epoll_create1);
  rc = (int)saved_sys_epoll_create1(flags);
  return rc;
}

static asmlinkage typeof(sys_epoll_ctl) *saved_sys_epoll_ctl;

int efab_linux_sys_epoll_ctl(int epfd, int op, int fd,
                             struct epoll_event *event)
{
  int rc;
  SET_SYSCALL_NO(epoll_ctl);
  rc = (int)saved_sys_epoll_ctl(epfd, op, fd, event);
  return rc;
}

static asmlinkage typeof(sys_epoll_pwait) *saved_sys_epoll_pwait;

int efab_linux_sys_epoll_wait(int epfd, struct epoll_event *events,
                              int maxevents, int timeout)
{
  int rc;
  SET_SYSCALL_NO(epoll_pwait);
  rc = (int)saved_sys_epoll_pwait(epfd, events, maxevents, timeout,
                                  NULL, sizeof(sigset_t));
  return rc;
}
#endif

static asmlinkage typeof(sys_sendmsg) *saved_sys_sendmsg;

int efab_linux_sys_sendmsg(int fd, struct msghdr __user* msg,
                           unsigned long __user* socketcall_args,
                           unsigned flags)
{
  int rc;
  SET_SYSCALL_NO(sys_sendmsg);
  rc = (int)saved_sys_sendmsg(fd, msg, socketcall_args, flags);
  return rc;
}


/* SPDX-License-Identifier: GPL-2.0 */
/* X-SPDX-Copyright-Text: (c) Solarflare Communications Inc */
/**************************************************************************\
*//*! \file
** <L5_PRIVATE L5_HEADER >
** \author  Tom Kelly
**  \brief  Unix syscall interface
**   \date  2003/12/18
**    \cop  (c) Level 5 Networks Limited.
** </L5_PRIVATE>
*//*
\**************************************************************************/

/*! \cidoxg_include_ci_ul */

#ifndef __CI_UL_SYSCALL_UNIX_H__
#define __CI_UL_SYSCALL_UNIX_H__

#if defined(__unix__) && defined(__GNUC__)

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <signal.h>

#include "libc_compat.h"
#include <ci/internal/transport_config_opt.h>


#if CI_CFG_RECVMMSG && ! defined(MSG_WAITFORONE)
/* recvmmsg() is a special case, because we want to support it even if it
 * is not supported in the system libc headers.
 */
# define OO_RECVMMSG_NOT_IN_LIBC 1
# define MSG_WAITFORONE  0x10000
#endif

#if CI_CFG_SENDMMSG && !CI_LIBC_HAS_sendmmsg
/* sendmmsg() is a similar special case */
# define OO_SENDMMSG_NOT_IN_LIBC 1
#endif

#if defined(OO_RECVMMSG_NOT_IN_LIBC) && defined(OO_SENDMMSG_NOT_IN_LIBC)
struct mmsghdr {
  struct msghdr  msg_hdr;
  unsigned       msg_len;
};
#endif

#if CI_LIBC_HAS___read_chk
extern ssize_t __read_chk (int fd, void *buf, size_t nbytes, size_t buflen);
#endif
#if CI_LIBC_HAS___recv_chk
extern ssize_t __recv_chk (int fd, void *buf, size_t nbytes, size_t buflen,
                           int flags);
#endif
#if CI_LIBC_HAS___recvfrom_chk
extern ssize_t __recvfrom_chk (int fd, void *buf, size_t nbytes, size_t buflen,
                              int flags, struct sockaddr*, socklen_t*);
#endif
#if CI_LIBC_HAS___poll_chk
extern int __poll_chk (struct pollfd *__fds, nfds_t __nfds, int __timeout,
                       size_t __fdslen);
#endif
#if CI_LIBC_HAS___ppoll_chk
extern int __ppoll_chk (struct pollfd *__fds, nfds_t __nfds,
                        const struct timespec *, const sigset_t *,
                       size_t __fdslen);
#endif

extern __sighandler_t bsd_signal(int signum, __sighandler_t handler);
extern __sighandler_t sysv_signal(int signum, __sighandler_t handler);


/*! Generate declarations of pointers to the system calls */
#define CI_MK_DECL(ret,fn,args)  extern ret (*ci_sys_##fn) args CI_HV
# include <onload/declare_syscalls.h.tmpl>


#ifdef OO_RECVMMSG_NOT_IN_LIBC
extern int ci_sys_recvmmsg(int fd, struct mmsghdr* msg, unsigned vlen,
                           int flags, const struct timespec* timeout);
#endif

#ifdef OO_SENDMMSG_NOT_IN_LIBC
extern int ci_sys_sendmmsg(int fd, struct mmsghdr* msg, unsigned vlen,
                           int flags);
#endif


#ifdef _STAT_VER
#define ci_sys_fstat(__fd, __statbuf)                          \
         ci_sys___fxstat(_STAT_VER, (__fd), (__statbuf))
# ifdef __USE_LARGEFILE64
#  define ci_sys_fstat64(__fd, __statbuf)                       \
          ci_sys___fxstat64(_STAT_VER, (__fd), (__statbuf))
# endif
#endif


#endif /* defined(__unix__) && defined(__GNUC__) */
#endif  /* __CI_UL_SYSCALL_UNIX_H__ */

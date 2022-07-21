/* SPDX-License-Identifier: GPL-2.0 */
/* X-SPDX-Copyright-Text: (c) Solarflare Communications Inc */
/**************************************************************************\
*//*! \file
** <L5_PRIVATE L5_HEADER >
** \author  mjs
**  \brief  Decls needed for async signal management.
**   \date  2005/03/06
**    \cop  (c) Level 5 Networks Limited.
** </L5_PRIVATE>
*//*
\**************************************************************************/

/*! \cidoxg_include_ci_internal  */
#ifndef __CI_INTERNAL_IP_SIGNAL_H__
#define __CI_INTERNAL_IP_SIGNAL_H__

#include <onload/signals.h>
#include <onload/ul/per_thread.h>
#include <ci/internal/tls.h>


#define OO_PRINT_SIGACTION_FMT "%p flags %x%s%s%s%s%s%s%s mask %lx"
#define OO_PRINT_SIGACTION_ARG(sa) \
  (sa)->sa_handler, (sa)->sa_flags, \
  (sa)->sa_flags & SA_NOCLDSTOP ? " NOCLDSTOP" : "", \
  (sa)->sa_flags & SA_NOCLDWAIT ? " NOCLDWAIT" : "", \
  (sa)->sa_flags & SA_SIGINFO   ? " SIGINFO"   : "", \
  (sa)->sa_flags & SA_ONSTACK   ? " ONSTACK"   : "", \
  (sa)->sa_flags & SA_RESTART   ? " RESTART"   : "", \
  (sa)->sa_flags & SA_NODEFER   ? " NODEFER"   : "", \
  (sa)->sa_flags & SA_RESETHAND ? " RESETHAND" : "", \
  (sa)->sa_mask.__val[0]

#if ! CI_CFG_USERSPACE_SYSCALL
#define ci_sys_syscall syscall
#endif


typedef struct oo_sig_thread_state citp_signal_info;


extern void citp_signal_run_pending(citp_signal_info* info) CI_HF;

ci_inline citp_signal_info *citp_signal_get_specific_inited(void)
{
  struct oo_per_thread* pt = __oo_per_thread_get();
  return &pt->sig;
}

extern int oo_do_sigaction(int sig, const struct sigaction *act,
                           struct sigaction *oldact);
extern int oo_syscall_sigaction(int sig, const struct sigaction* user_act,
                                struct sigaction* user_oldact);

/* Signal initialization is performed in 2 stages:
 * - install SIGONLOAD handler and find sa_restorer
 *   oo_sigonload_init, called from citp_fdtable_ctor
 *   CITP_INIT_FDTABLE;
 * - intercept all terminating SIG_DFL, all early-called signal() calls,
 *   all libc-protected signals such as SIGCANCEL
 *   oo_init_signals(), the last init bit
 *   CITP_INIT_SIGNALS
 */
extern int oo_sigonload_init(void* handler);
extern int oo_init_signals(void);

#endif  /* __CI_INTERNAL_IP_SIGNAL_H__ */
/*! \cidoxg_end */

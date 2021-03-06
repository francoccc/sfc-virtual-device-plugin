/* SPDX-License-Identifier: GPL-2.0 */
/* X-SPDX-Copyright-Text: (c) Solarflare Communications Inc */
/**************************************************************************\
*//*! \file
** <L5_PRIVATE L5_SOURCE>
** \author  djr
**  \brief  Slow path for eplocks.  (Lock contended case).
**   \date  2003/02/14
**    \cop  (c) Level 5 Networks Limited.
** </L5_PRIVATE>
*//*
\**************************************************************************/

/*! \cidoxg_lib_transport_ip */
#include <ci/internal/ip.h>
#include <ci/internal/ip_log.h>

#ifndef __KERNEL__
# include <onload/ul.h>
# include "ip_internal.h"
#endif


CI_BUILD_ASSERT( (CI_EPLOCK_LOCK_FLAGS & CI_EPLOCK_CALLBACK_FLAGS) == 0 );


static int __oo_eplock_lock(ci_netif* ni, long* timeout, int maybe_wedged)
{
#ifndef __KERNEL__
  /* timeout is in long to keep jiffies in kernel, but it is int32
   * in UL keeping msecs.
   */
  ci_int32 op = (long)*timeout;
  ci_assert_equal(maybe_wedged, 0);
  return oo_resource_op(ci_netif_get_driver_handle(ni), OO_IOC_EPLOCK_LOCK,
                        &op);
#else
  return oo_eplock_lock(ni, timeout, maybe_wedged);
#endif

}

int __ef_eplock_lock_slow(ci_netif *ni, long timeout, int maybe_wedged)
{
#ifndef __KERNEL__
  ci_uint64 start_frc, now_frc;
#endif
  int rc;

#ifndef __KERNEL__
  ci_assert_equal(maybe_wedged, 0);
#endif

#ifndef __KERNEL__
  /* Limit to user-level for now.  Could allow spinning in kernel if we did
   * not rely on user-level accessible state for spin timeout.
   */
  if( oo_per_thread_get()->spinstate & (1 << ONLOAD_SPIN_STACK_LOCK) ) {
    CITP_STATS_NETIF(++ni->state->stats.stack_lock_buzz);
    ci_frc64(&now_frc);
    start_frc = now_frc;
    while( now_frc - start_frc < ni->state->buzz_cycles ) {
      ci_spinloop_pause();
      ci_frc64(&now_frc);
      if( ef_eplock_trylock(&ni->state->lock) )
        return 0;
    }
  }
#endif

  while( 1 ) {
    rc = __oo_eplock_lock(ni, &timeout, maybe_wedged);
    if( rc == 0 || rc == -ETIMEDOUT )
      return rc;

#ifndef __KERNEL__
    if( rc == -EINTR )
      /* Keep waiting if interrupted by a signal.  I think this is okay:
       * If the outer call blocks, we'll handle the signal before
       * blocking, and behave as if the signal arrived before the outer
       * call.  If the outer call does not block, then we'll handle the
       * signal on return, and behave as if the signal arrived after the
       * outer call.
       */
      continue;
    /* This should never happen. */
    LOG_E(ci_log("%s: ERROR: rc=%d", __FUNCTION__, rc));
    CI_TEST(0);
#else
    /* There is nothing we can do except propagate the error.  Caller
     * must handle it.
     */
    if( (rc == -ERESTARTSYS) || (rc == -ECANCELED) )
      return rc;
    LOG_E(ci_log("%s: ERROR: rc=%d", __FUNCTION__, rc));
    return rc;
#endif
  }

  /* Can't get here. */
  ci_assert(0);
  return 0;
}

/*! \cidoxg_end */

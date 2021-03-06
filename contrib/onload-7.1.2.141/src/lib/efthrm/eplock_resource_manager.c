/* SPDX-License-Identifier: GPL-2.0 */
/* X-SPDX-Copyright-Text: (c) Solarflare Communications Inc */
/**************************************************************************\
*//*! \file
** <L5_PRIVATE L5_SOURCE>
** \author  
**  \brief  
**   \date  
**    \cop  (c) Level 5 Networks Limited.
** </L5_PRIVATE>
*//*
\**************************************************************************/
  
/*! \cidoxg_driver_efab */
 
#include <ci/internal/ip.h>
#include <onload/debug.h>
#include <onload/tcp_helper_fns.h>

#if !defined(CI_HAVE_COMPARE_AND_SWAP)
# error Need cas for this code.
#endif



int
eplock_ctor(ci_netif *ni)
{
  init_waitqueue_head(&ni->eplock_helper.wq);
  ni->state->lock.lock = 0;

#if CI_CFG_EFAB_EPLOCK_RECORD_CONTENTIONS
  /* if asked we keep a record of who waited on this lock */
  for(i=0; i < EFAB_EPLOCK_MAX_NO_PIDS; i++) {
    ni->eplock_helper.pids_who_waited[i] = 0;
    ni->eplock_helper.pids_no_waits[i] = 0;
  }
#endif

  return 0;
}


void
eplock_dtor(ci_netif *ni)
{
}


#if CI_CFG_EFAB_EPLOCK_RECORD_CONTENTIONS
static void
efab_eplock_record_pid(ci_netif *ni)
{
  ci_irqlock_state_t lock_flags;
  int i;
  int index = -1;

  int pid = current->pid;

  ci_irqlock_lock(&ni->eplock_helper.pids_lock, &lock_flags);
  for (i=0; i < EFAB_EPLOCK_MAX_NO_PIDS; i++) {

    if (ni->eplock_helper.pids_who_waited[i] == pid) {
      ni->eplock_helper.pids_no_waits[i]++;
      ci_irqlock_unlock(&ni->eplock_helper.pids_lock, &lock_flags);

      return;
    }

    if ((rs->pids_who_waited[i] == 0) && (index < 0)) {
      index = i;
    }
  }
  if (index >= 0) {
    ni->eplock_helper.pids_who_waited[index] = pid;
    ni->eplock_helper.pids_no_waits[index] = 1;
  }
  ci_irqlock_unlock(&ni->eplock_helper.pids_lock, &lock_flags);
}
#endif



/**********************************************************************
 * CI_RESOURCE_OPs.
 */

int
efab_eplock_unlock_and_wake(ci_netif *ni, int in_dl_context)
{
  ci_uint64 l = ni->state->lock.lock;

  /* We use in_dl_context from now on, and we should remove
   * CI_NETIF_FLAG_IN_DL_CONTEXT under the stack lock. */
  if( in_dl_context )
    ni->flags &= ~CI_NETIF_FLAG_IN_DL_CONTEXT;

 again:

  if( l & CI_EPLOCK_CALLBACK_FLAGS ) {
    /* Invoke the callback while we've still got the lock.  The callback
    ** is responsible for either
    **  - dropping the lock using ef_eplock_try_unlock(), and returning 
    **    the lock value prior to unlocking, OR
    **  - keeping the eplock locked and returning CI_EPLOCK_LOCKED
    */
    l = efab_tcp_helper_netif_lock_callback(&ni->eplock_helper, l, in_dl_context);
  }
  else if( ci_cas64u_fail(&ni->state->lock.lock, l, 0) ) {
    /* Someone (probably) set a flag when we tried to unlock, so we'd
    ** better handle the flag(s).
    */
    l = ni->state->lock.lock;
    goto again;
  }

  if( l & CI_EPLOCK_FL_NEED_WAKE ) {
    CITP_STATS_NETIF_INC(ni, lock_wakes);
    wake_up_interruptible(&ni->eplock_helper.wq);
  }

  return 0;
}


static int efab_eplock_is_unlocked_or_request_wake(ci_eplock_t* epl)
{
  ci_uint64 l;

  while( (l = epl->lock) & CI_EPLOCK_LOCKED )
    if( (l & CI_EPLOCK_FL_NEED_WAKE) ||
        ci_cas64u_succeed(&epl->lock, l, l | CI_EPLOCK_FL_NEED_WAKE) )
      return 1;

  ci_assert_nflags(l, CI_EPLOCK_LOCKED);

  return 0;
}

/* Returns 0 if lock have been taken; 1 otherwise */
static int oo_eplock_lock_or_request_wake(ci_eplock_t* epl)
{
  ci_uint64 l;

  do {
    l = epl->lock;
    if( l & CI_EPLOCK_LOCKED ) {
      if( (l & CI_EPLOCK_FL_NEED_WAKE) ||
          ci_cas64u_succeed(&epl->lock, l, l | CI_EPLOCK_FL_NEED_WAKE) )
        return 1;
    }
    else {
      if( ci_cas64u_succeed(&epl->lock, l, l | CI_EPLOCK_LOCKED) )
        return 0;
    }
  } while(1);

  /* Can't get here. */
  ci_assert(0);
  return -EFAULT;
}

int
efab_eplock_lock_wait(ci_netif* ni, int maybe_wedged)
{
  wait_queue_entry_t wait;
  int rc;

#if CI_CFG_EFAB_EPLOCK_RECORD_CONTENTIONS
  efab_eplock_record_pid(ni);
#endif

  init_waitqueue_entry(&wait, current);
  add_wait_queue(&ni->eplock_helper.wq, &wait);

  while( 1 ) {
    set_current_state(TASK_INTERRUPTIBLE);
    rc = efab_eplock_is_unlocked_or_request_wake(&ni->state->lock);
    if( rc <= 0 )
      break;
    schedule();
    if(CI_UNLIKELY( signal_pending(current) )) {
      rc = -ERESTARTSYS;
      break;
    }
    if(CI_UNLIKELY(maybe_wedged) ) {
      rc = -ECANCELED;
      break;
    }
  }

  remove_wait_queue(&ni->eplock_helper.wq, &wait);
  set_current_state(TASK_RUNNING);
  return rc;
}

int oo_eplock_lock(ci_netif* ni, long* timeout_jiffies, int maybe_wedged)
{
  wait_queue_entry_t wait;
  int rc;

#if CI_CFG_EFAB_EPLOCK_RECORD_CONTENTIONS
  efab_eplock_record_pid(ni);
#endif

  init_waitqueue_entry(&wait, current);
  add_wait_queue(&ni->eplock_helper.wq, &wait);

  while( 1 ) {
    set_current_state(TASK_INTERRUPTIBLE);
    rc = oo_eplock_lock_or_request_wake(&ni->state->lock);
    if( rc <= 0 )
      break;
    *timeout_jiffies = schedule_timeout(*timeout_jiffies);
    if( *timeout_jiffies == 0 ) {
      rc = -ETIMEDOUT;
      break;
    }
    if(CI_UNLIKELY( signal_pending(current) )) {
      rc = -ERESTARTSYS;
      break;
    }
    if(CI_UNLIKELY(maybe_wedged) ) {
      rc = -ECANCELED;
      break;
    }
  }

  remove_wait_queue(&ni->eplock_helper.wq, &wait);
  set_current_state(TASK_RUNNING);
  return rc;
}


/* For in-kernel use only.  Ignores signals.
 * Tries to lock netif for the given number of jiffies.
 * Fails in case of timeout. */
int
efab_eplock_lock_timeout(ci_netif* ni, signed long timeout_jiffies)
{
  wait_queue_entry_t wait;
  int rc = -EAGAIN;

  if( ef_eplock_trylock(&ni->state->lock) )
    return 0;

  init_waitqueue_entry(&wait, current);
  add_wait_queue(&ni->eplock_helper.wq, &wait);

  while( timeout_jiffies > 0 ) {
    if( efab_eplock_is_unlocked_or_request_wake(&ni->state->lock) )
      timeout_jiffies = schedule_timeout_interruptible(timeout_jiffies);
    if( ef_eplock_trylock(&ni->state->lock) ) {
      rc = 0;
      break;
    }
  }
  remove_wait_queue(&ni->eplock_helper.wq, &wait);
  return rc;
}

/*! \cidoxg_end */

/* SPDX-License-Identifier: GPL-2.0 */
/* X-SPDX-Copyright-Text: (c) Solarflare Communications Inc */
/**************************************************************************\
** <L5_PRIVATE L5_SOURCE>
**   Copyright: (c) Level 5 Networks Limited.
**      Author: ctk
**     Started: 2004/03/23
** Description: User level TCP helper interface.
** </L5_PRIVATE>
\**************************************************************************/

/*! \cidoxg_include_ci_driver_efab  */

#ifndef __CI_DRIVER_EFAB_TCP_HELPER_H__
#define __CI_DRIVER_EFAB_TCP_HELPER_H__


#include <ci/compat.h>
#include <ci/internal/ip.h>
#include <onload/osfile.h>
#include <onload/oof_hw_filter.h>
#include <onload/oof_socket.h>


/* Forwards. */
typedef struct tcp_helper_endpoint_s tcp_helper_endpoint_t;


struct tcp_helper_nic {
  int                  thn_intf_i;
  struct oo_nic*       thn_oo_nic;
  struct efrm_vi*      thn_vi_rs;
#if CI_CFG_SEPARATE_UDP_RXQ
  struct efrm_vi*      thn_udp_rxq_vi_rs;
#endif
  /* Track the size of the VI mmap in the kernel. */
  unsigned             thn_vi_mmap_bytes;
#if CI_CFG_SEPARATE_UDP_RXQ
  unsigned             thn_udp_rxq_vi_mmap_bytes;
#endif
#if CI_CFG_PIO
  struct efrm_pio*     thn_pio_rs;
  unsigned             thn_pio_io_mmap_bytes;
#endif
#if CI_CFG_CTPIO
  unsigned             thn_ctpio_io_mmap_bytes;
  void*                thn_ctpio_io_mmap;
#endif
};


/* Keeps reference to os socket bound to an ephemeral port.
 * This is to allow reuse in multiple stacks.
 *
 * These ports will be later reused by shared local ports in more than one
 * stack (in case of cluster).
 * The structure below is assumed to have single reference to os file.  Other
 * reference holders are shared local port endpoints in one or more stacks.
 * The reference owned by the port_keeper is to be freed last after all the
 * endpoints are destroyed. */
struct efab_ephemeral_port_keeper {
   /* Instances of this structure live, in the general case, on two linked
    * lists simultaneously.  One, linked by [next], is specific to a single IP
    * address (where that IP address will be INADDR_ANY when shared local ports
    * are not per-IP), and the other, linked by [global_next], is common to all
    * IP addresses. */
   struct efab_ephemeral_port_keeper* next;
   struct efab_ephemeral_port_keeper* global_next;
   struct socket* sock;
   struct file* os_file;
   ci_addr_t laddr;
   uint16_t port_be16;
};

/* List-head for a list of efab_ephemeral_port_keeper structures. */
struct efab_ephemeral_port_head {
  struct efab_ephemeral_port_keeper* head;
  struct efab_ephemeral_port_keeper** tail_next_ptr;
  /* Pointer into the list of all (i.e. not just for this IP) local ports
   * indicating the point up to which it has been consumed by this local IP in
   * our attempts to use ports already allocated for other IPs. */
  struct efab_ephemeral_port_keeper* global_consumed;
  ci_addr_t laddr;
  uint32_t port_count;
};

struct tcp_helper_resource_s;

typedef struct tcp_helper_cluster_s {
  struct efrm_vi_set*             thc_vi_set[CI_CFG_MAX_HWPORTS];
  ci_dllist                       thc_thr_list;
  struct tcp_helper_resource_s**  thc_thr_rrobin;
  /* Indicates which stack in the round robin to switch to next. */
  int                             thc_thr_rrobin_index;
#define THC_REHEAT_FLAG_USE_SWITCH_PORT 0x1
#define THC_REHEAT_FLAG_STICKY_MODE     0x2
  unsigned                        thc_reheat_flags;
  /* Port used to determine if we should switch stack for this bind. */
  ci_addr_t                       thc_switch_addr;
  uint16_t                        thc_switch_port;
  char                            thc_name[CI_CFG_CLUSTER_NAME_LEN + 1];
  int                             thc_cluster_size;
  oo_atomic_t                     thc_thr_count;
  uid_t                           thc_keuid;
  ci_dllist                       thc_tlos;

#define THC_FLAG_PACKET_BUFFER_MODE 0x1
#define THC_FLAG_HW_LOOPBACK_ENABLE 0x2

/* Various RSS hash settings, note that they are targetted at TCP. */
#define THC_FLAG_TPROXY             0x4
#define THC_FLAG_SCALABLE          0x10

#define THC_FLAG_PREALLOC_LPORTS   0x20
  unsigned                        thc_flags;
  uint16_t*                       thc_tproxy_ifindex;
  int                             thc_tproxy_ifindex_count;

  struct tcp_helper_cluster_s*    thc_next;
  oo_atomic_t                     thc_ref_count;

  wait_queue_head_t               thr_release_done;

  struct oo_cplane_handle*        thc_cplane;
  struct oo_filter_ns*            thc_filter_ns;

  /* The ephemeral ports owned by the cluster are maintained in a hash table
   * keyed by local IP address. */
  struct efab_ephemeral_port_head* thc_ephem_table;
  uint32_t                         thc_ephem_table_entries;
} tcp_helper_cluster_t;


/* Used to backup and, if necessary restore, state during a reuseport bind. */
typedef struct tcp_helper_reheat_state_s {
  int       thr_prior_index;
  pid_t     thc_tid_effective;
  unsigned  thc_reheat_flags;
  ci_addr_t thc_switch_addr;
  unsigned  thc_switch_port;
} tcp_helper_reheat_state_t;


 /*--------------------------------------------------------------------
 *
 * tcp_helper_resource_t
 *
 *--------------------------------------------------------------------*/

/*! Comment? */
typedef struct tcp_helper_resource_s {
  /* A number of fields here duplicate fields in ci_netif_state.  This is
   * deliberate, and is because we do not trusted the contents of the
   * shared state.
   */
  unsigned               id;
  char                   name[CI_CFG_STACK_NAME_LEN + 1];
  oo_atomic_t            ref_count;

  ci_netif               netif;

  /*! Kernel side stack lock. Needed so we can determine who "owns" the
   *   netif lock (kernel or user).
   *
   * The flags can only be set when the lock is LOCKED.  ie. This must be
   * UNLOCKED, or LOCKED possibly in combination with the other flags.  If
   * AWAITING_FREE is set, other flags must not be.
   */
#define OO_TRUSTED_LOCK_UNLOCKED          0x0
#define OO_TRUSTED_LOCK_LOCKED            0x1
#define OO_TRUSTED_LOCK_AWAITING_FREE     0x2
#define OO_TRUSTED_LOCK_NEED_POLL         0x4
#define OO_TRUSTED_LOCK_CLOSE_ENDPOINT    0x8
#define OO_TRUSTED_LOCK_OS_READY          0x10
#define OO_TRUSTED_LOCK_NEED_PRIME        0x20
/* 0x40 is available for reuse; was OO_TRUSTED_LOCK_DONT_BLOCK_SHARED. */
#define OO_TRUSTED_LOCK_SWF_UPDATE        0x80
#define OO_TRUSTED_LOCK_PURGE_TXQS        0x100
  volatile unsigned      trusted_lock;

  /*! Link for global list of stacks. */
  ci_dllink              all_stacks_link;

  /*! A count of kernel references to this stack.  Normally there is one
   * reference to indicate that this stack is still referenced by userland.
   * Some other bits of code may hold a reference.
   *
   * Once the userland reference has gone away we set
   * TCP_HELPER_K_RC_NO_USERLAND to prevent new user-level references being
   * taken.
   *
   * When the ref count goes to zero we set TCP_HELPER_K_RC_DEAD to ensure
   * other code won't grab another ref to this stack.
   */
  volatile int           k_ref_count;
# define TCP_HELPER_K_RC_NO_USERLAND    0x10000000
# define TCP_HELPER_K_RC_DEAD           0x20000000
# define TCP_HELPER_K_RC_REFS(krc)      ((krc) & 0xffffff)

  /* A count of the refs added to k_ref_count for closing endpoints in
   * efab_tcp_helper_rm_free_locked().  Protected by thr->lock *not*
   * ci_netif lock */
  int n_ep_closing_refs;

  /*! this is used so we can schedule destruction at task time */
  struct work_struct work_item_dtor;
  struct completion complete;

  /* For pinning periodic work */
  int periodic_timer_cpu;

  /* For deferring work to a non-atomic context. */
#define ONLOAD_WQ_NAME "onload-wq:%s"
#define ONLOAD_WQ_NAME_BASELEN 11
  char wq_name[ONLOAD_WQ_NAME_BASELEN + ONLOAD_PRETTY_NAME_MAXLEN];
  struct workqueue_struct *wq;
  struct work_struct non_atomic_work;
  /* List of endpoints requiring work in non-atomic context. */
  ci_sllist     non_atomic_list;

  /* For deferring resets to a non-atomic context. */
#define ONLOAD_RESET_WQ_NAME "onload-rst-wq:%s"
#define ONLOAD_RESET_WQ_NAME_BASELEN 15
  char reset_wq_name[ONLOAD_RESET_WQ_NAME_BASELEN + ONLOAD_PRETTY_NAME_MAXLEN];
  struct workqueue_struct *reset_wq;
  struct work_struct reset_work;

#define ONLOAD_PERIODIC_WQ_NAME "onload-periodic-wq:%s"
#define ONLOAD_PERIODIC_WQ_NAME_BASELEN 20
  char periodic_wq_name[ONLOAD_PERIODIC_WQ_NAME_BASELEN + ONLOAD_PRETTY_NAME_MAXLEN];
  struct workqueue_struct *periodic_wq;
  struct delayed_work purge_txq_work;

#ifdef CONFIG_NAMESPACES

/* Use nsproxy for RHEL6, because free_pid_ns() is not exported here */
#if defined(RHEL_MAJOR)
#if RHEL_MAJOR == 6
#define OO_USE_NSPROXY
#endif
#endif

#define EFRM_DO_NAMESPACES
#ifdef OO_USE_NSPROXY
  struct nsproxy *nsproxy;
#else
  /* Namespaces this stack is living into */
  struct net* net_ns;
  struct pid_namespace* pid_ns;
#ifdef ERFM_HAVE_NEW_KALLSYMS
#define OO_HAS_IPC_NS
  /* put_ipc_ns() is not exported, so we can't use it without
   * kallsyms_on_each_symbol() */
  struct ipc_namespace* ipc_ns;
#endif /* ERFM_HAVE_NEW_KALLSYMS */
#endif /* OO_USE_NSPROXY */
#endif /* CONFIG_NAMESPACES */

#ifdef EFRM_DO_USER_NS
  struct user_namespace* user_ns;
#endif

#ifdef  __KERNEL__
  /*! clear to indicate that timer should not restart itself */
  atomic_t                 timer_running;
  /*! timer - periodic poll */
  struct delayed_work      timer;


#endif  /* __KERNEL__ */

  /*! tcp_helper endpoint(s) to be closed at next calling of
   * linux_tcp_helper_fop_close() or if tcp_helper_resource is released
   */
  ci_sllist             ep_tobe_closed;

  /* This field is currently used under the trusted lock only, BUT it must
   * be modified via atomic operations ONLY.  These flags are used in
   * stealing the shared and trusted locks from atomic or driverlink
   * context to workqueue context.  When such a "steal" (or "deferral")
   * is in action, the field might be used from 2 contexts 
   * simultaneously. */
  volatile ci_uint32    trs_aflags;
  /* We've deferred locks to non-atomic handler.  Must close endpoints. */
# define OO_THR_AFLAG_CLOSE_ENDPOINTS     0x1
  /* We've deferred locks to non-atomic handler.  Must poll and prime. */
# define OO_THR_AFLAG_POLL_AND_PRIME      0x2
  /* We've deferred locks to non-atomic handler.  Must unlock only. */
# define OO_THR_AFLAG_UNLOCK_TRUSTED      0x4
  /* Have we deferred something while holding the trusted lock? */
# define OO_THR_AFLAG_DEFERRED_TRUSTED    0x7

  /* Defer the shared lock (without the trusted lock!) to the work queue */
# define OO_THR_AFLAG_UNLOCK_UNTRUSTED    0x8

  /* Don't block on the shared lock when resetting a stack. */
# define OO_THR_AFLAG_DONT_BLOCK_SHARED   0x10

  /*! Spinlock.  Protects:
   *    - ep_tobe_closed
   *    - non_atomic_list
   *    - wakeup_list
   *    - n_ep_closing_refs
   *    - intfs_to_reset 
   *    - intfs_to_xdp_update
   */
  ci_irqlock_t          lock;

  /* Bit mask of intf_i that need resetting by the lock holder */
  unsigned              intfs_to_reset;
  /* Bit mask of intf_i that have been removed/suspended and not yet reset */
  unsigned              intfs_suspended;

  unsigned              mem_mmap_bytes;
  unsigned              io_mmap_bytes;
  unsigned              buf_mmap_bytes;
#if CI_CFG_PIO
  /* Length of the PIO mapping.  There is typically a page for each VI */
  unsigned              pio_mmap_bytes;
#endif
#if CI_CFG_CTPIO
  /* Length of the CTPIO mapping.  The same one is used for all VIs. */
  unsigned              ctpio_mmap_bytes;
#endif

  /* Used to block threads that are waiting for free pkt buffers. */
  ci_waitq_t            pkt_waitq;
  
  struct tcp_helper_nic      nic[CI_CFG_MAX_INTERFACES];

  /* The cluster this stack is associated with if any */
  tcp_helper_cluster_t*         thc;
  /* TID of thread that created this stack within the cluster */
  pid_t                         thc_tid;
  /* TID of thread with right to do sticky binds on this stack in reheat mode */
  pid_t                         thc_tid_effective;
  /* Track list of stacks associated with a single thc */
  ci_dllink             thc_thr_link;
  /* bucket of rss hardware filter */
  int thc_rss_instance;

#ifdef ONLOAD_OFE
  struct mutex ofe_mutex;
  struct ofe_configurator* ofe_config;
#endif

  ci_waitable_t         ready_list_waitqs[CI_CFG_N_READY_LISTS];
  ci_dllist             os_ready_lists[CI_CFG_N_READY_LISTS];
  spinlock_t            os_ready_list_lock;

  struct oo_filter_ns*  filter_ns;

  uint16_t*             tproxy_ifindex;
  int                   tproxy_ifindex_count;

  /* The available ephemeral ports for active wilds are maintained in a hash
   * table keyed by local IP address.  If the stack is clustered, then this
   * table is shared by all stacks in the cluster. */
  struct efab_ephemeral_port_head* trs_ephem_table;
  uint32_t                         trs_ephem_table_entries;
  /* We also need to remember the point in each list beyond which the ports
   * have already been consumed.  We use a hash table keyed in the same way.
   * N.B.: While the table of ports may be shared between stacks, the tracking
   * of consumed ports is always per-stack. */
  struct efab_ephemeral_port_head* trs_ephem_table_consumed;
} tcp_helper_resource_t;


#define NI_OPTS_TRS(trs) (NI_OPTS(&(trs)->netif))

#define netif2tcp_helper_resource(ni)                   \
  CI_CONTAINER(tcp_helper_resource_t, netif, (ni))

#ifdef NDEBUG
#define TCP_HELPER_RESOURCE_ASSERT_VALID(trs, rc_mbz)
#else
extern void tcp_helper_resource_assert_valid(tcp_helper_resource_t*,
                                             int rc_is_zero,
                                             const char *file, int line);
#define TCP_HELPER_RESOURCE_ASSERT_VALID(trs, rc_mbz) \
    tcp_helper_resource_assert_valid(trs, rc_mbz, __FILE__, __LINE__)
#endif


 /*--------------------------------------------------------------------
 *
 * tcp_helper_endpoint_t
 *
 *--------------------------------------------------------------------*/

/*! Information about endpoint accessible to kernel only */
struct tcp_helper_endpoint_s {

  /*! TCP helper resource we are a part of */
  tcp_helper_resource_t * thr;

  /*! Endpoint ID */
  oo_sp id;

  /*! Per-socket state for the filter manager. */
  struct oof_socket oofilter;

  /*! OS socket responsible for port reservation; may differ from os_socket
   * (for accepted socket) and is set/cleared together with filters.
   * Concurrency control is via atomic exchange (oo_file_ref_xchg()).
   */
  struct oo_file_ref* os_port_keeper;

  /*! link so we can be in the list of endpoints to be closed in the future */
  ci_sllink tobe_closed;

  /* Link field when queued for non-atomic work. */
  ci_sllink non_atomic_link;

  /*! Links of the list with endpoints with pinned pages */
  ci_dllink ep_with_pinned_pages;
  /*! List of pinned pages */
  ci_dllist pinned_pages;
  /*! Number of pinned pages */
  unsigned int n_pinned_pages;

  /*! Head of the waitqueue */
  ci_waitable_t waitq;			

  /* IRQ lock to protect os_socket.
   * It is not ci_irqlock_t, because ci_irqlock_t is BH lock, but we need
   * IRQ lock here.  This lock is used from Linux wake up callback, and
   * __wake_up() function calls spin_lock_irqsave() before calling
   * callbacks. */
  spinlock_t lock;

  /*!< OS socket that backs this user-level socket.  May be NULL (not all
   * socket types have an OS socket).
   * os_socket and os_sock_pt should be changed under ep->lock only.
   */
  struct oo_file_ref* os_socket;

  /*!< Used to poll OS socket for OS events. */
  struct oo_os_sock_poll os_sock_poll;

  struct fasync_struct* fasync_queue;

  /*! Link for the wakeup list.  This *must* be reset to zero when not in
  ** use.
  */
  tcp_helper_endpoint_t* wakeup_next;

  /*! Atomic endpoint flags not visible for UL. */
  volatile ci_uint32 ep_aflags;
#define OO_THR_EP_AFLAG_PEER_CLOSED    0x2  /* Used for pipe */
#define OO_THR_EP_AFLAG_NON_ATOMIC     0x4  /* On the non-atomic list */
#define OO_THR_EP_AFLAG_CLEAR_FILTERS  0x8  /* Needs filters clearing */
#define OO_THR_EP_AFLAG_NEED_FREE      0x10 /* Endpoint to be freed */
#define OO_THR_EP_AFLAG_OS_NOTIFIER    0x20 /* Pollwait registration for os */

  struct ci_private_s* alien_ref;

  struct {
    ci_dllink os_ready_link;
  } epoll[CI_CFG_N_READY_LISTS];


  /*! Back pointer to handle cases where cleaning up requires
  **  a file object, and not a handle, because all handles
  *   have been closed by that point.
  *   Note: this is a weak pointer and does not refcount the file.
  */
  ci_os_file file_ptr;

};


#ifdef __KERNEL__

#ifdef EFRM_DO_NAMESPACES

#ifdef OO_USE_NSPROXY
#define thr_net_ns(thr) ((thr)->nsproxy->net_ns)
#define thr_pid_ns(thr) ((thr)->nsproxy->pid_ns)
#else
#define thr_net_ns(thr) ((thr)->net_ns)
#define thr_pid_ns(thr) ((thr)->pid_ns)
#endif

static inline struct pid_namespace*
ci_get_pid_ns(struct nsproxy* proxy)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,11,0))
  return proxy->pid_ns_for_children;
#else
  return proxy->pid_ns;
#endif
}

/* Return the PID namespace in which the given ci_netif lives.
 *
 * Note that for the lifetime of the stack, thr->nsproxy is a counted
 * reference, therefore it can't become NULL while this stack is still
 * alive. */
ci_inline struct pid_namespace* ci_netif_get_pidns(ci_netif* ni)
{
  tcp_helper_resource_t* thr = netif2tcp_helper_resource(ni);
#ifdef OO_USE_NSPROXY
  return ci_get_pid_ns(thr->nsproxy);
#else
  return thr->pid_ns;
#endif
}

/* Log an error and return failure if the current process is not in
 * the right namespaces to operate on the given stack. */
ci_inline int ci_netif_check_namespace(ci_netif* ni)
{
  tcp_helper_resource_t* thr = netif2tcp_helper_resource(ni);
  if( ni == NULL ) {
    ci_log("In ci_netif_check_namespace() with ni == NULL");
    return -EINVAL;
  }
  if( current == NULL ) {
    ci_log("In ci_netif_check_namespace() outside process context");
    return -EINVAL;
  }
  if( current->nsproxy == NULL ) {
    ci_log("In ci_netif_check_namespace() without valid namespaces");
    return -EINVAL;
  }
  if( (thr_net_ns(thr) != current->nsproxy->net_ns) ||
      (thr_pid_ns(thr) != ci_get_pid_ns(current->nsproxy)) )
  {
    ci_log("NAMESPACE MISMATCH: pid %d accessed a foreign stack",
           current->pid);
    return -EINVAL;
  }
  return 0;
}

#endif /* defined(EFRM_DO_NAMESPACES) */

/* Look up a PID in this ci_netif's PID namespace. Must be called with
 * the tasklist_lock or rcu_read_lock() held. */
ci_inline struct pid* ci_netif_pid_lookup(ci_netif* ni, pid_t pid)
{
#ifdef EFRM_DO_NAMESPACES
  struct pid_namespace* ns = ci_netif_get_pidns(ni);
  return find_pid_ns(pid, ns);
#else
  return find_vpid(pid);
#endif
}

/* Given an index into the nic_hw array, return the kernel's ifindex.
 * Implemented as a macro rather than an inline function, to avoid required
 * dependencies on additional headers in this file */
#define ci_trs_get_ifindex(thr, intf_i)                 \
  (efrm_client_get_ifindex((thr)->nic[(intf_i)].thn_vi_rs->rs.rs_client))

#endif /* defined(__KERNEL__) */


#endif /* __CI_DRIVER_EFAB_TCP_HELPER_H__ */
/*! \cidoxg_end */

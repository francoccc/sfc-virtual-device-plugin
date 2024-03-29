load("@rules_cc//cc:defs.bzl", "cc_import", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_import(
    name = "libzocket",
    shared_library = "prebuilt/gnu_x86_64/lib/zf/libonload_zf.so",
    static_library = "prebuilt/gnu_x86_64/lib/zf/libonload_zf_static.a",
)

cc_library(
    name = "zocket",
    hdrs = glob([
        "src/include/zf/*.h",
        "src/include/zf/**/*.h",
    ]),
    strip_include_prefix = "src/include",
    deps = [
        ":libzocket",
    ],
)

cc_library(
    name = "ciul_lib",
    srcs = glob([
        "src/lib/ciul/*.c",
        "src/lib/ciul/*.h",
        "src/lib/ci/efhw/*.h",
        "src/include/etherfabric/*.h",
        "src/include/etherfabric/**/*.h",
        "src/include/ci/*.h",
        "src/include/ci/**/*.h",
    ]) + [
        "efch_intf_ver.h",
        "src/include/onload/version.h",
    ],
    hdrs = glob([
        "src/lib/ciul/*.h",
        "src/lib/ci/efhw/*.h",
    ]),
    copts = [
        "-Iexternal/onload/src/include",
    ],
    strip_include_prefix = "src/lib",
)

cc_library(
    name = "ciul",
    hdrs = glob([
        "src/include/etherfabric/*.h",
        "src/include/etherfabric/**/*.h",
        "src/include/onload/*.h",
        "src/include/onload/**/*.h",
        "src/include/ci/*.h",
        "src/include/ci/**/*.h",
    ]),
    copts = [
        "-Iexternal/onload/src/include",
    ],
    strip_include_prefix = "src/include",
    deps = [
        ":ciul_lib",
    ]
)

cc_library(
    name = "ci_tool",
    srcs = glob(
        [
            "src/include/ci/tools/**/*.h",
            "src/include/ci/compat/*.h",
            "src/include/ci/net/*.h",
            "src/lib/citools/*.c",
            "src/lib/citools/*.h",
        ],
        exclude = [
            "src/lib/citools/toeplitz.c",
            "src/lib/citools/drv_thread.c",
            "src/lib/citools/drv_log_fn.c",
            "src/lib/citools/memleak_debug.c",
        ],
    ) + [
        "src/include/ci/tools.h",
        "src/include/ci/compat.h",
    ],
    hdrs = glob([
        "src/include/ci/tools/*.h",
        "src/include/ci/compat/*.h",
        "src/include/ci/tools/**/*.h",
        "src/include/ci/net/*.h",
    ]) + [
        "src/include/ci/tools.h",
        "src/include/ci/compat.h",
    ],
    copts = [
        "-Iexternal/onload/src/include",
    ],
    deps = [
        "//:ciul",
    ],
    strip_include_prefix = "src/include",
)

genrule(
    name = "ciul_efch_intf_ver_h",
    outs = ["efch_intf_ver.h"],
    cmd = "\n".join([
        "cat << 'EOF' >$@",
        "#define EFCH_INTF_VER  \"5c1c482de0fe41124c3dddbeb0bd5a1a\"",
        "EOF",
    ]),
)

genrule(
    name = "autocompat_h",
    outs = ["driver/linux_affinity/autocompat.h"],
    cmd = "\n".join([
        "cat << 'EOF' >$@",
        "#define EFRM_HAVE_NSPROXY yes",
        "#define EFRM_HAVE_NETDEV_NOTIFIER_INFO yes",
        "#define EFRM_HAVE_IOREMAP_WC yes",
        "#define EFRM_HAVE_IOMMU_GROUP yes",
        "#define EFRM_HAVE_KSTRTOUL yes",
        "#define EFRM_HAVE_KSTRTOL yes",
        "#define EFRM_HAVE_IN4_PTON yes",
        "#define EFRM_HAVE_IN6_PTON yes",
        "#define EFRM_HAVE_STRCASECMP yes",
        "#define EFRM_HAVE_REINIT_COMPLETION yes",
        "#define EFRM_HAVE_GET_UNUSED_FD_FLAGS yes",
        "#define EFRM_HAVE_WQ_SYSFS yes",
        "#define EFRM_HAVE_POLL_REQUESTED_EVENTS yes",
        "#define ERFM_HAVE_NEW_KALLSYMS yes",
        "// #define EFRM_HAVE_TASK_NSPROXY",
        "// #define EFRM_HAVE_IOMMU_DOMAIN_HAS_CAP",
        "#define EFRM_HAVE_IOMMU_CAPABLE yes",
        "#define EFRM_HAVE_TEAMING yes",
        "#define EFRM_HAVE_CLOEXEC_TEST yes",
        "// #define EFRM_HAVE_SET_RESTORE_SIGMASK",
        "#define EFRM_HAVE_SET_RESTORE_SIGMASK1 yes",
        "// #define EFRM_FSTYPE_HAS_INIT_PSEUDO",
        "#define EFRM_FSTYPE_HAS_MOUNT_PSEUDO yes",
        "#define EFRM_HAVE_KERN_UMOUNT yes",
        "#define EFRM_HAVE_ALLOC_FILE_PSEUDO yes",
        "// #define EFRM_HAVE_KMEM_CACHE_S",
        "#define EFRM_HAVE_SCHED_TASK_H yes",
        "#define EFRM_HAVE_CRED_H yes",
        "#define EFRM_HAVE_NF_NET_HOOK yes",
        "// #define EFRM_HAVE_USERMODEHELPER_SETUP",
        "// #define EFRM_RTMSG_IFINFO_EXPORTED",
        "#define EFRM_HAVE_NS_SYSCTL_TCP_MEM yes",
        "#define EFRM_HAVE_KERNEL_PARAM_OPS yes",
        "#define EFRM_HAVE_TIMER_SETUP yes",
        "#define EFRM_HAVE_READ_SEQCOUNT_LATCH yes",
        "#define EFRM_HAVE_WRITE_SEQCOUNT_LATCH yes",
        "#define EFRM_HAVE_RBTREE yes",
        "#define EFRM_HAVE_SKB_METADATA yes",
        "#define EFRM_HAVE_BIN2HEX yes",
        "#define EFRM_HAVE_ALLSYMS_SHOW_VALUE yes",
        "#define EFRM_HAVE_ARRAY_SIZE yes",
        "#define EFRM_HAVE_WRITE_ONCE yes",
        "#define EFRM_HAVE_INIT_LIST_HEAD_RCU yes",
        "#define EFRM_HAVE_S_MIN_MAX yes",
        "// #define EFRM_ACCESS_OK_HAS_2_ARGS",
        "#define EFRM_IP6_ROUTE_INPUT_LOOKUP_EXPORTED yes",
        "#define EFRM_HAVE_DEV_GET_IF_LINK yes",
        "#define EFRM_HAVE_FILE_INODE yes",
        "#define EFRM_HAVE_UNMAP_KERNEL_RANGE yes",
        "// #define EFRM_HAVE_PRANDOM_U32",
        "#define EFRM_HAVE_GET_RANDOM_LONG yes",
        "#define EFRM_HAS_KTIME_GET_TS64 yes",
        "// #define EFRM_HAVE_MMAP_LOCK_WRAPPERS",
        "// #define EFRM_HAVE_SOCK_BINDTOINDEX",
        "#define EFRM_HAS_REMAP_VMALLOC_RANGE_PARTIAL yes",
        "#define EFRM_HAS_KTIME_GET_REAL_SECONDS yes",
        "// #define EFRM_HAS_LOOKUP_FD_RCU",
        "// #define EFRM_HAVE_PROC_CREATE",
        "// #define EFRM_HAVE_PROC_CREATE_DATA",
        "#define EFRM_HAVE_PROC_CREATE_DATA_UMODE yes",
        "// #define EFRM_HAVE_PROC_CREATE_DATA_PROC_OPS",
        "#define EFRM_HAVE_PDE_DATA yes",
        "// #define EFRM_OLD_DEV_BY_IDX",
        "#define EFRM_HAVE_PGPROT_WRITECOMBINE yes",
        "// #define EFRM_HAVE_IOMMU_MAP_OLD",
        "#define EFRM_HAVE_IOMMU_MAP yes",
        "// #define EFRM_HAVE_NETFILTER_INDIRECT_SKB",
        "// #define EFRM_HAVE_NETFILTER_HOOK_OPS",
        "#define EFRM_HAVE_NETFILTER_HOOK_STATE yes",
        "// #define EFRM_HAVE_NETFILTER_OPS_HAVE_OWNER",
        "// #define EFRM_POLL_TABLE_HAS_OLD_KEY",
        "// #define EFRM_HAVE_F_DENTRY",
        "#define EFRM_HAVE_MSG_ITER yes",
        "// #define EFRM_SOCK_SENDMSG_NEEDS_LEN",
        "// #define EFRM_SOCK_RECVMSG_NEEDS_BYTES",
        "#define EFRM_HAVE_FOP_READ_ITER yes",
        "#define EFRM_SOCK_CREATE_KERN_HAS_NET yes",
        "#define EFRM_HAVE_SK_SLEEP_FUNC yes",
        "// #define EFRM_ALLOC_FILE_TAKES_STRUCT_PATH",
        "// #define EFRM_ALLOC_FILE_TAKES_CONST_STRUCT_PATH",
        "// #define EFRM_NEED_VFSMOUNT_PARAM_IN_GET_SB",
        "// #define EFRM_HAVE_KMEM_CACHE_DTOR",
        "// #define EFRM_HAVE_KMEM_CACHE_FLAGS",
        "// #define EFRM_HAVE_KMEM_CACHE_CACHEP",
        "// #define EFRM_NET_HAS_PROC_INUM",
        "#define EFRM_NET_HAS_USER_NS yes",
        "// #define EFRM_HAVE_OLD_FAULT",
        "#define EFRM_HAVE_NEW_FAULT yes",
        "// #define EFRM_OLD_NEIGH_UPDATE",
        "#define EFRM_HAVE_WAIT_QUEUE_ENTRY yes",
        "// #define EFRM_GUP_RCINT_TASK_SEPARATEFLAGS",
        "// #define EFRM_GUP_RCLONG_TASK_SEPARATEFLAGS",
        "// #define EFRM_GUP_RCLONG_TASK_COMBINEDFLAGS",
        "#define EFRM_GUP_RCLONG_NOTASK_COMBINEDFLAGS yes",
        "// #define EFRM_HAVE_USERMODEHELPER_SETUP_INFO",
        "#define EFRM_RTMSG_IFINFO_NEEDS_GFP_FLAGS yes",
        "#define EFRM_DEV_GET_BY_NAME_TAKES_NS yes",
        "#define EFRM_HAVE_CONST_KERNEL_PARAM yes",
        "// #define EFRM_DO_COREDUMP_BINFMTS_SIGNR",
        "// #define EFRM_DO_COREDUMP_COREDUMP_SIGNR",
        "#define EFRM_RTNL_LINK_OPS_HAS_GET_LINK_NET yes",
        "#define EFRM_HAVE_DEVICE_DEVNODE_UMODE yes",
        "#define EFRM_MAP_VM_AREA_TAKES_PAGESTARSTAR yes",
        "#define EFRM_IP6_ROUTE_INPUT_LOOKUP_TAKES_SKB yes",
        "// #define EFRM_RTABLE_HAS_RT_GW4",
        "#define EFRM_HAS_STRUCT_TIMEVAL yes",
        "#define EFRM_HAS_STRUCT_TIMESPEC64 yes",
        "// #define EFRM_HAVE_STRUCT_PROC_OPS",
        "// #define EFRM_MSGHDR_HAS_MSG_CONTROL_USER",
        "// #define EFRM_HAS_SOCKPTR",
        "#define EFRM_REMAP_VMALLOC_RANGE_PARTIAL_NEW yes",
        "// #define EFRM_FILE_HAS_F_EP",
        "// #define EFRM_HAS_ITER_TYPE",
        "EOF",
    ]),
)

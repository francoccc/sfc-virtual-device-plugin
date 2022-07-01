# sfc-virtual-device-plugin

## Introduction:
The virtual SFC device plugin is a DaemonSet of the Kubernetes Cluster.Provide a TX/RX-like queue for every pod so that client can use DMA send or receive under less memory copy consumption.Bazel supports the whole project for easy compiling, using, and deploying.

## Environment:
1. gcc9 (or later)
2. OpenOnload (v7.1.2.141)
3. Linux_x86_64
4. bazel (4.0.0)
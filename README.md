<!--
 * @Descripition: Intro
 * @Author: Franco Chen
 * @Date: 2022-07-21 17:29:21
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-07-22 10:43:01
-->
# sfc-virtual-device-plugin

## Introduction:
The virtual SFC device plugin is a DaemonSet of the Kubernetes Cluster.Provide a TX/RX-like queue for every pod so that client can use DMA send or receive under less memory copy consumption.Bazel supports the whole project for easy compiling, using, and deploying.

## Environment:
1. gcc9 (or later)
2. OpenOnload (v7.1.2.141)
3. Linux_x86_64
4. bazel (4.0.0)

## Setup:
For applying this plugin, make sure you are under similar enviroment and the first step is to build a local network driver of solarflare(sfc_char).
  ```
    contrib/onload/scripts/onload_build --kernel
  ```
Then install the new sfc_char.ko we built. The following command will overwrite the previous sfc_char.ko.
  ```
    cp -f contrib/onload/build/$(uname -m)_$(uname -s |tr '[A-Z]' '[a-z]')-$(uname -r)/driver/linux/sfc_char.ko \ 
    /usr/lib/modules/$(uname -r)/extra
    
    rmmod sfc_char
    modprobe sfc_char
  ```
Until now, we have changed the kernel drive and need to build the device plugin tarball and import it by docker. You can also push the image tarball to the registry (@see //plugin:publish_image).
  ```
    bazel build //plugin:vsfc_plugin_img
    docker import bazel-bin/plugin/vsfc_plugin_img-layer.tar
  ```
Finally, apply the Daemonset and alloc a new vsfc-nic resource in pod yaml.
  ```
    kubectl apply -f deploy/vsfc-cdp.yaml
    
    POD-YAML:
    apiVersion: v1
    kind: Pod
    metadata:
      name: pod
    spec:
      containers:
      - name: pod_name
        image: image_tar
        command: ["..."]
        args: ["..."]
        resources:
          limits:
            highfortfunds.com/vsfc: 1
  ```
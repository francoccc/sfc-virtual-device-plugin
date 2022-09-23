/*
 * @Descripition:
 * @Author: Franco Chen
 * @Date: 2022-09-22 14:18:35
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-09-22 16:08:13
 */
package main

import (
	"context"
	"fmt"
	"time"

	"k8s.io/kubernetes/pkg/kubelet/apis/podresources"
	podresourcesapi "k8s.io/kubernetes/pkg/kubelet/apis/podresources/v1alpha1"
	"k8s.io/kubernetes/pkg/kubelet/util"
)

const (
	defaultPodResourcesMaxSize = 1024 * 1024 * 16 // 16 Mb
	defaultPodResourcesPath = "/var/lib/kubelet/pod-resources"
)

func main() {
	kubeletSocket, _ := util.LocalEndpoint(defaultPodResourcesPath, podresources.Socket)
	client, conn, err := podresources.GetClient(kubeletSocket, 10 * time.Second, defaultPodResourcesMaxSize)
	if err != nil {
		panic("GetClient error")
	}
	defer conn.Close()
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	resp, err := client.List(ctx, &podresourcesapi.ListPodResourcesRequest{})
	if err != nil {
		panic(fmt.Errorf("ListPodResources error: %v", err))
	}
	fmt.Println("n: ", len(resp.PodResources))
}

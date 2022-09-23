/*
 * @Descripition:
 * @Author: Franco Chen
 * @Date: 2022-09-23 11:38:49
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-09-23 13:16:14
 */
package main

import (
	"context"
	"fmt"

	netclient "github.com/k8snetworkplumbingwg/network-attachment-definition-client/pkg/client/clientset/versioned/typed/k8s.cni.cncf.io/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/rest"
)


func main() {
	config, err := rest.InClusterConfig()
	if err != nil {
		fmt.Println(err)
		return
	}
	netclient, err := netclient.NewForConfig(config)
	if err != nil {
		fmt.Println(err)
		return
	}
	nad, err := netclient.NetworkAttachmentDefinitions("default").Get(context.TODO(), "gw-inter", metav1.GetOptions{})
	if err != nil {
		fmt.Println(err)
		return
	}
	fmt.Printf("%#v\n", nad)
	nad.Annotations["test"] = "netclient"
	if _, err := netclient.NetworkAttachmentDefinitions("default").Update(context.TODO(), nad, metav1.UpdateOptions{}); err != nil {
		fmt.Println(err)
		return
	}
}

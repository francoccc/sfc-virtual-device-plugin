/*
 * @Descripition: k8s-client-go includes
 * @Author: Franco Chen
 * @Date: 2022-09-20 18:00:44
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-09-23 15:46:52
 */
package client

import (
	"context"
	"encoding/json"

	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/clientcmd"

	"sfc-virtual-device-plugin/plugin/config"

	nettypes "github.com/k8snetworkplumbingwg/network-attachment-definition-client/pkg/apis/k8s.cni.cncf.io/v1"
	netclient "github.com/k8snetworkplumbingwg/network-attachment-definition-client/pkg/client/clientset/versioned/typed/k8s.cni.cncf.io/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

const (
	resourceNameAnnot = "k8s.v1.cni.cncf.io/resourceName"
)

type ClientInfo struct {
	Client        kubernetes.Interface
	NetClient     netclient.K8sCniCncfIoV1Interface
}

func (c *ClientInfo) AddNetAttachDefByResource(resource, resourceName string) (*nettypes.NetworkAttachmentDefinition, error) {
	nadc := config.NetAttachDefConf{
		CNIVersion: "0.3.0",
		Name: resourceName,
		Plugins: []config.Plugin{
			{
				Type: "sfc-macvlan",
				Mode: "bridge",
			},
		},
	}

	bytes, _ := json.Marshal(nadc)
	nad := nettypes.NetworkAttachmentDefinition{
		ObjectMeta: metav1.ObjectMeta{
			Name: resourceName,
			Annotations: map[string]string{
				resourceNameAnnot: resource,
			},
		},
		Spec: nettypes.NetworkAttachmentDefinitionSpec{
			Config: string(bytes),
		},
	}
	return c.AddNetAttachDef(&nad)
}

func (c *ClientInfo) GetDefaultNetAttachDef(name string) (*nettypes.NetworkAttachmentDefinition, error) {
	return c.GetNetAttachDef("default", name)
}

func (c *ClientInfo) GetNetAttachDef(namespace, name string) (*nettypes.NetworkAttachmentDefinition, error) {
	return c.NetClient.NetworkAttachmentDefinitions(namespace).Get(context.TODO(), name, metav1.GetOptions{})
}

func (c *ClientInfo) AddNetAttachDef(netattach *nettypes.NetworkAttachmentDefinition) (*nettypes.NetworkAttachmentDefinition, error) {
	return c.NetClient.NetworkAttachmentDefinitions(netattach.ObjectMeta.Namespace).Create(context.TODO(), netattach, metav1.CreateOptions{})
}

func (c *ClientInfo) UpdateNetAttachDef(netattach *nettypes.NetworkAttachmentDefinition) (*nettypes.NetworkAttachmentDefinition, error) {
	return c.NetClient.NetworkAttachmentDefinitions(netattach.ObjectMeta.Namespace).Update(context.TODO(), netattach, metav1.UpdateOptions{})
 }

func InClusterK8sClient() (*ClientInfo, error) {
	config, err := rest.InClusterConfig()
	if err != nil {
		return nil, err
	}
	return NewClientInfo(config)
}

func NewLocalClient() (*ClientInfo, error) {
	config, err:= clientcmd.BuildConfigFromFlags("", "/etc/solarflare/vsfc-cdp.kubeconfig")
	if err != nil {
		return nil, err
	}
	return NewClientInfo(config)
}

func NewClientInfo(config *rest.Config) (*ClientInfo, error) {
	client, err := kubernetes.NewForConfig(config)
	if err != nil {
		return nil, err
	}
	netclient, err := netclient.NewForConfig(config)
	if err != nil {
		return nil, err
	}

	return &ClientInfo{
		Client:    client,
		NetClient: netclient,
	}, nil
}

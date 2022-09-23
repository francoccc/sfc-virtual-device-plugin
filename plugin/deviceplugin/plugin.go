/*
 * @Descripition: generic plugin.go
 * @Author: Franco Chen
 * @Date: 2022-06-27 11:37:55
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-09-22 16:43:46
 */
package deviceplugin

import (
	"context"
	"fmt"
	"net"
	"path/filepath"
	"strings"
	"time"

	k8s "sfc-virtual-device-plugin/plugin/client"

	"github.com/golang/glog"
	"google.golang.org/grpc"
	pluginapi "k8s.io/kubelet/pkg/apis/deviceplugin/v1beta1"
)

// var mutex sync.Mutex
const (
	restartInterval     = time.Duration(5) * time.Second
)

func NewPluginBase(impl PluginImpl) Plugin {
	return &PluginBase{
		PluginImpl: impl,
	}
}

func MakeUnixSockPath(pluginDir, resource string) string {
	return filepath.Join(
		pluginDir,
		fmt.Sprintf(
			"%s-%d.sock",
			strings.Replace(resource, "/", "~", -1),
			time.Now().Unix(),
		),
	)
}

func (plugin *PluginBase) Run(ctx context.Context, k8sclient *k8s.ClientInfo) error {
	Outer:
		for {
			select {
			case <-ctx.Done():
				break Outer
			default:
				if err := plugin.PluginImpl.RunOnce(ctx, k8sclient); err != nil {
					glog.Info("encountered error while running plugin; trying again in 5 seconds", " err: ", err)
					select {
					case <-ctx.Done():
						break Outer
					case <-time.After(restartInterval):
						glog.Info("Restarting...")
					}
				}
			}
		}
		return plugin.PluginImpl.CleanUp()
}

func (plugin *PluginBase) registerWithKubelet() error {
	glog.Info("registering plugin with kubelet")
	conn, err := grpc.Dial(
		filepath.Join(plugin.pluginDir, filepath.Base(pluginapi.KubeletSocket)),
		grpc.WithInsecure(),
		grpc.WithBlock(),
		grpc.WithDialer(func(addr string, timeout time.Duration) (net.Conn, error) {
			return net.DialTimeout("unix", addr, timeout)
		}),
	)
	if err != nil {
		return fmt.Errorf("failed to connect to kubelet: %v", err)
	}
	defer conn.Close()

	client := pluginapi.NewRegistrationClient(conn)
	request := &pluginapi.RegisterRequest{
		Version:      pluginapi.Version,
		Endpoint:     filepath.Base(plugin.socket),
		ResourceName: string(plugin.resource),
	}
	if _, err = client.Register(context.Background(), request); err != nil {
		return fmt.Errorf("failed to register plugin with kubelet service: %v", err)
	}
	glog.Info("registered plugin with kubelet succ")
	return nil
}

func (plugin *PluginBase) CleanUp() error {
	return plugin.PluginImpl.CleanUp()
}

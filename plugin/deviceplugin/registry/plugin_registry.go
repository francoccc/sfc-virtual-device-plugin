/*
 * @Descripition: Plugin registry
 * @Author: Franco Chen
 * @Date: 2022-09-16 16:18:40
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-09-20 14:45:42
 */
package registry

import (
	"context"
	"fmt"
	"sync"

	"sfc-virtual-device-plugin/plugin/config"
	"sfc-virtual-device-plugin/plugin/deviceplugin"

	"github.com/golang/glog"
)

var (
	ResourceRegistry = map[string]func(string, string) deviceplugin.PluginImpl{
		"highfortfunds.com/vsfc": deviceplugin.NewPlugin,
		"highfortfunds.com/vsfc-mem": deviceplugin.NewMemVSfcPlugin,
		"highfortfunds.com/vsfc-handle": deviceplugin.NewPlugin,
	}

	// configUpdateEvent => configUpdate => child
	configUpdate chan struct{}
	child chan<- struct{}
	mu sync.Mutex
)

func Transfer(channel chan struct{}) error {
	if channel == nil {
		return fmt.Errorf("invalid arguments")
	}
	child = channel
	return nil
}

func Run(ctx context.Context) error {
	go syncLoop(ctx)
	configUpdate = make(chan struct{})
	if err := config.Subscribe("pluginRegistry", configUpdate); err != nil {
		return err
	}
	return nil
}

func syncLoop(ctx context.Context) {
	for {
		select {
		case <-configUpdate:
			for _, inter := range config.Conf().Interfaces {
				if !hasResource(inter.Resource) {
					if inter.Parent == "" {
						glog.Error("this resource need a parent resource: ", inter.Resource)
						continue
					}
					if !hasResource(inter.Parent) {
						glog.Error("invalid parent resource: ", inter.Parent)
						continue
					}
					ResourceRegistry[inter.Resource] = ResourceRegistry[inter.Parent]
					glog.Info("generate resource: ", inter.Resource, " inherit from ", inter.Parent)
				}
			}
			child <- struct{}{}
		case <-ctx.Done():
			return
		}
	}
}

func hasResource(resource string) bool {
	_, ok := ResourceRegistry[resource]
	return ok
}

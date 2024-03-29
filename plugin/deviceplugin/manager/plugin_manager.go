/*
 * @Descripition: plugin manager
 * @Author: Franco Chen
 * @Date: 2022-09-16 10:36:58
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-09-23 10:38:40
 */
package manager

import (
	"context"
	"sync"

	k8s "sfc-virtual-device-plugin/plugin/client"
	"sfc-virtual-device-plugin/plugin/config"
	"sfc-virtual-device-plugin/plugin/deviceplugin"
	"sfc-virtual-device-plugin/plugin/deviceplugin/registry"
	"sfc-virtual-device-plugin/plugin/resource/types"

	"github.com/golang/glog"
)


type PluginMgr interface {
	CleanUp()
	Run(ctx context.Context, autoConf bool, stopChan <-chan struct{}) error
}

type pluginMgr struct {
	wg sync.WaitGroup
	plugins map[string]deviceplugin.Plugin
	pluginDir string
	pluginUpdate chan struct{}
	k8sclient *k8s.ClientInfo
}

func NewPluginMgr(pluginDir string) (PluginMgr, error) {
	k8sclient, err := k8s.InClusterK8sClient()
	if err != nil {
		return nil, err
	}
	return &pluginMgr{
		plugins: make(map[string]deviceplugin.Plugin),
		pluginDir: pluginDir,
		pluginUpdate: make(chan struct{}),
		k8sclient: k8sclient,
	}, nil
}

func (mgr *pluginMgr) Run(ctx context.Context, autoConf bool, stopChan <-chan struct{}) error {
	ctx, cancel := context.WithCancel(ctx)
	// run with configUpdate
	go mgr.syncLoop(ctx)

	if autoConf {
		// use pluginRegistry
		glog.Info("use plugin registry to maintain resources and plugins")
		registry.Transfer(mgr.pluginUpdate)
		registry.Run(ctx)
	} else {
		config.Subscribe("pluginMgr", mgr.pluginUpdate)
	}

	<-stopChan
	cancel()
	mgr.wg.Wait()
	return nil
}

func (mgr *pluginMgr) CleanUp() {
	// clean up all unix sock
	for _, plugin := range mgr.plugins {
		plugin.CleanUp()
	}
}

func (mgr *pluginMgr) syncLoop(ctx context.Context) error {
	glog.Info("start syncLoop")
	for {
		select {
		case <-mgr.pluginUpdate:
			glog.V(8).Info("config updated")
			for _, inter := range config.Conf().Interfaces {
				glog.V(8).Info("update interface ", inter)
				constructor, ok := registry.ResourceRegistry[types.Resource(inter.Resource)]
				if !ok {
					glog.Errorf("plugin not found by resource: %v", inter.Resource)
					continue
				}
				if _, ok := mgr.plugins[inter.Resource]; !ok {
					glog.Info("create and run plugin:", inter.Resource)
					plugin, err := constructor(inter.Resource, mgr.pluginDir)
					if err != nil {
						glog.Errorf("failed to construct plugin [resource: %q] err: %v", inter.Resource, err)
						continue
					}
					mgr.plugins[inter.Resource] = deviceplugin.NewPluginBase(plugin)
					mgr.runPlugin(ctx, inter.Resource)
				}
			}
		case <-ctx.Done():
			glog.Info("plugin manager exit sync loop")
			return nil
		}
	}
}

func (mgr *pluginMgr) runPlugin(ctx context.Context, resource string) {
	glog.Infof("run a goroutine of plugin [resouce: %q]", resource)
	go func() {
		mgr.wg.Add(1)
		mgr.plugins[resource].Run(ctx, mgr.k8sclient)
		mgr.wg.Done()
		// TODO: mgr.plugins remove?
	} ()
}

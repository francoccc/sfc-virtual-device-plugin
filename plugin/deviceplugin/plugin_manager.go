/*
 * @Descripition: plugin manager
 * @Author: Franco Chen
 * @Date: 2022-09-16 10:36:58
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-09-19 15:17:20
 */
package deviceplugin

import (
	"context"
	"sync"

	"sfc-virtual-device-plugin/plugin/config"

	"github.com/golang/glog"
)


type PluginMgr interface {
	CleanUp()
	Run(ctx context.Context, stopChan <-chan struct{}) error
}

type pluginMgr struct {
	wg sync.WaitGroup
	plugins map[string]Plugin
	pluginDir string
	pluginUpdate chan struct{}
}

func NewPluginMgr(pluginDir string) (PluginMgr, error) {
	return &pluginMgr{
		plugins: make(map[string]Plugin),
		pluginDir: pluginDir,
		pluginUpdate: make(chan struct{}),
	}, nil
}

func (mgr *pluginMgr) Run(ctx context.Context, stopChan <-chan struct{}) error {
	ctx, cancel := context.WithCancel(ctx)


	// run with configUpdate
	go mgr.syncLoop(ctx)
	config.Subscribe("pluginMgr", mgr.pluginUpdate)

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
	glog.Info("enter syncLoop")
	for {
		select {
		case <-mgr.pluginUpdate:
			glog.V(8).Info("config updated")
			for _, inter := range config.Conf().Interfaces {
				glog.V(8).Info("update interface ", inter)
				constructor, ok := ResourceRegistry[inter.Resource]
				if !ok {
					glog.Errorf("plugin not found by resource: %v", inter.Resource)
					continue
				}
				if _, ok := mgr.plugins[inter.Resource]; !ok {
					glog.Info("create and run plugin:", inter.Resource)
					plugin := constructor(inter.Resource, mgr.pluginDir)
					mgr.plugins[inter.Resource] = NewPluginBase(plugin)
					mgr.runPlugin(ctx, inter.Resource)
				}
			}
		case <-ctx.Done():
			glog.Info("plugin manager exit sync loop")
			break
		default:
		}
	}
	glog.Info("exit syncLoop")
	return nil
}

func (mgr *pluginMgr) runPlugin(ctx context.Context, resource string) {
	go func() {
		mgr.wg.Add(1)
		mgr.plugins[resource].Run(ctx)
		mgr.wg.Done()
	} ()
}

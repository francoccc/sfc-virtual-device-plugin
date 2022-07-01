/*
 * @Descripition: virtual sfc deviceplugin main entrypoint
 * @Author: Franco Chen
 * @Date: 2022-06-06 17:30:44
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-06-29 15:15:08
 */
package main

import (
	"context"
	"flag"
	"os"
	"plugin/deviceplugin"
	"plugin/util"

	"github.com/golang/glog"
	pluginapi "k8s.io/kubelet/pkg/apis/deviceplugin/v1beta1"
)

var (
	// efviClient efvi.EfviServiceClient
	addr = flag.String("addr", "localhost:8088", "efvi_allocator local address")
	pluginPath = flag.String("plugin-directory", pluginapi.DevicePluginPath, "The directory in which to create plugin socket")
	resourceName = flag.String("resource-name", "highfortfunds.com/vsfc", "The name of plugin resource")
)

func main() {
	flag.Parse()
	
	flag.Lookup("logtostderr").Value.Set("true")
	
	glog.Info("Start")
	util.ForkProcess("/app/efvi_allocator", []string{})
	
	vsfc := deviceplugin.NewVSFCPluginServer(*resourceName, *pluginPath)
	ctx, _ := context.WithCancel(context.Background())
	vsfc.Run(ctx)
	os.Exit(1)  // died
}

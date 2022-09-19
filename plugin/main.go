/*
 * @Descripition: virtual sfc deviceplugin main entrypoint
 * @Author: Franco Chen
 * @Date: 2022-06-06 17:30:44
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-09-19 10:15:48
 */
package main

import (
	"context"
	"flag"
	"fmt"
	"os"
	"sfc-virtual-device-plugin/plugin/config"
	"sfc-virtual-device-plugin/plugin/deviceplugin"
	"sfc-virtual-device-plugin/plugin/util"

	"github.com/golang/glog"
	pluginapi "k8s.io/kubelet/pkg/apis/deviceplugin/v1beta1"
)

var (
	// efviClient efvi.EfviServiceClient
	allocServiceAddr = flag.String("addr", "localhost:8088", "The address of efvi_allocator")
	pluginPath = flag.String("plugin-directory", pluginapi.DevicePluginPath, "The directory in which to create plugin socket")
	resourceName = flag.String("resource-name", "highfortfunds.com/vsfc", "The name of plugin resource")
	resourceHandleName = flag.String("resource-handle-name", "highfortfunds.com/vsfc-handle", "The name of plugin resource handle")
)

func main() {
	flag.Parse()

	flag.Lookup("logtostderr").Value.Set("true")

	glog.Info("Start")
	util.ForkProcess("/app/efvi_allocator", []string{})

	config.Run(context.Background())

	pluginMgr, err := deviceplugin.NewPluginMgr(*pluginPath)
	if err != nil {
		fmt.Println("new plugin manager err: ", err)
		os.Exit(1)
	}

	// never stop
	pluginMgr.Run(context.Background(), make(chan struct{}))
	defer pluginMgr.CleanUp()

	os.Exit(1)  // died
}

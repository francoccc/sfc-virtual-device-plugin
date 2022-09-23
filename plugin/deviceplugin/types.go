package deviceplugin

import (
	"context"
	k8s "sfc-virtual-device-plugin/plugin/client"
	"sfc-virtual-device-plugin/plugin/resource/types"
	"sync"

	pluginapi "k8s.io/kubelet/pkg/apis/deviceplugin/v1beta1"
)

// device wraps the v1.beta1.Device type to add context about
// the device needed by the GenericPlugin.
type MountDevice struct {
	pluginapi.Device
	Name string
	Nic string
	DeviceSpecs []*pluginapi.DeviceSpec
	Mounts      []*pluginapi.Mount
}

type Plugin interface {
	CleanUp() error
	Run(context.Context, *k8s.ClientInfo) error

	registerWithKubelet() error
}

type PluginImpl interface {
	CleanUp() error
	RunOnce(context.Context, *k8s.ClientInfo) error
}

type PluginBase struct {
	Plugin
	PluginImpl
	resource   types.Resource
	pluginDir  string
	socket     string
}

type PluginServerImpl interface {
	RefreshDevices() ([]MountDevice, error)
	TryAllocate([]*pluginapi.ContainerAllocateRequest) ([]*pluginapi.ContainerAllocateResponse, error)
}

// extend pluginapi.DevicePluginServer aka dps
type PluginServerBase struct {
	pluginapi.DevicePluginServer
	impl       PluginServerImpl
	mu         sync.Mutex
	devices    map[string]MountDevice
}

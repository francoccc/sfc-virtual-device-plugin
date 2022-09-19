package deviceplugin

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/golang/glog"

	// metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"

	pluginapi "k8s.io/kubelet/pkg/apis/deviceplugin/v1beta1"
)

const (
	deviceCheckInterval = 5 * time.Second
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

type PluginServerBase struct {
	pluginapi.DevicePluginServer
	impl       PluginServerImpl
	mu         sync.Mutex
	devices    map[string]MountDevice
}

// GetDeviceState always returns healthy.
func (pluginServer *PluginServerBase) GetDeviceState(_ string) string {
	return pluginapi.Healthy
}

// Allocate assigns generic devices to a Pod.
func (pluginServer *PluginServerBase) Allocate(_ context.Context, req *pluginapi.AllocateRequest) (*pluginapi.AllocateResponse, error) {
	pluginServer.mu.Lock()
	defer pluginServer.mu.Unlock()

	res := &pluginapi.AllocateResponse{}

	cap, err := pluginServer.impl.TryAllocate(req.ContainerRequests);
	if err != nil {
		return nil, err
	}
	res.ContainerResponses = cap
	return res, nil
}

// GetDevicePluginOptions always returns an empty response.
func (pluginServer *PluginServerBase) GetDevicePluginOptions(_ context.Context, _ *pluginapi.Empty) (*pluginapi.DevicePluginOptions, error) {
	return &pluginapi.DevicePluginOptions{}, nil
}

// ListAndWatch lists all devices and then refreshes every deviceCheckInterval.
func (pluginServer *PluginServerBase) ListAndWatch(_ *pluginapi.Empty, stream pluginapi.DevicePlugin_ListAndWatchServer) error {
	glog.Info("kubelet starting listwatch")
	if _, err := pluginServer.refreshDevices(); err != nil {
		return err
	}
	ok := false
	var err error
	for {
		if !ok {
			res := new(pluginapi.ListAndWatchResponse)
			for _, dev := range pluginServer.devices {
				res.Devices = append(res.Devices, &pluginapi.Device{ID: dev.ID, Health: dev.Health})
			}
			if err := stream.Send(res); err != nil {
				return err
			}
		}
		<-time.After(deviceCheckInterval)
		ok, err = pluginServer.refreshDevices()
		if err != nil {
			return err
		}
	}
}

// PreStartContainer always returns an empty response.
func (pluginServer *PluginServerBase) PreStartContainer(_ context.Context, _ *pluginapi.PreStartContainerRequest) (*pluginapi.PreStartContainerResponse, error) {
	return &pluginapi.PreStartContainerResponse{}, nil
}

// GetPreferredAllocation always returns an empty response.
func (pluginServer *PluginServerBase) GetPreferredAllocation(context.Context, *pluginapi.PreferredAllocationRequest) (*pluginapi.PreferredAllocationResponse, error) {
	return &pluginapi.PreferredAllocationResponse{}, nil
}

func (pluginServer *PluginServerBase) refreshDevices() (bool, error) {
	devices, err := pluginServer.impl.RefreshDevices()
	if err != nil {
		return false, fmt.Errorf("failed to discover devices: %v", err)
	}

	pluginServer.mu.Lock()
	defer pluginServer.mu.Unlock()

	old := pluginServer.devices
	pluginServer.devices = make(map[string]MountDevice)
	var equal bool = true
	// Add the new devices to the map and check
	// if they were in the old map.
	for _, d := range devices {
		pluginServer.devices[d.ID] = d
		if _, ok := old[d.ID]; !ok {
			equal = false
		}
	}
	if !equal {
		glog.Info("update new devices count: ", len(pluginServer.devices))
		return false, nil
	}

	// Check if devices were removed.
	for k := range old {
		if _, ok := pluginServer.devices[k]; !ok {
			glog.Warning("found old device removed: ", k)
			return false, nil
		}
	}
	return true, nil
}

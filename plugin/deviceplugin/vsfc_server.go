package deviceplugin

import (
	"context"
	"crypto/sha1"
	"fmt"
	"sfc-virtual-device-plugin/efvi"
	k8s "sfc-virtual-device-plugin/plugin/client"
	"sfc-virtual-device-plugin/plugin/config"
	"sfc-virtual-device-plugin/plugin/resource"
	"sfc-virtual-device-plugin/plugin/resource/types"
	"sfc-virtual-device-plugin/plugin/util"
	"strconv"

	"github.com/golang/glog"
	"google.golang.org/grpc"
	pluginapi "k8s.io/kubelet/pkg/apis/deviceplugin/v1beta1"
)

const (
	vNicPerNic = 8
	sharedMemory = "/dev/shm/"
	hugepages    = "/mnt/hugepages/"
	defaultPermission = "mrw"
	viAllocNamePrefix = "vi"
	viContainerMountPath = "/mnt/vi"
	viHugePageQueueMountPath = "/mnt/vi/huge"
)

type vSfcPluginServer struct {
	PluginServerBase
	firstRunDiscover bool
	resource         types.Resource
	k8sclient        *k8s.ClientInfo
}

func NewVSfcPluginServer(resource string) pluginapi.DevicePluginServer {
	client, _ := k8s.InClusterK8sClient()
	server := &vSfcPluginServer{
		resource: types.Resource(resource),
		firstRunDiscover: true,
		k8sclient: client,
	}
	server.impl = server
	server.devices = make(map[string]MountDevice)
	return server
}

func (dps *vSfcPluginServer) RefreshDevices() ([]MountDevice, error) {
	var devices []MountDevice

	for index, inter := range util.FindSolarflareInterfaces() {
		found := false
		for _, configed := range config.Conf().Interfaces {
			if dps.resource == types.Resource(configed.Resource) && inter.Name == configed.Name {
				found = true
			}
		}
		if !found {
			// skip
			continue
		}
		if dps.firstRunDiscover {
			glog.Info("resource: ", dps.resource, " logical name: ", inter.Name)
		}
		for i := 1; i <= vNicPerNic; i++ {
			h := sha1.New()
			h.Write([]byte(inter.Name))
			h.Write([]byte(dps.resource))
			h.Write([]byte(strconv.FormatUint(uint64(i), 10)))
			d := MountDevice{
				Device: pluginapi.Device{
					Health: pluginapi.Healthy,
				},
				Name: viAllocNamePrefix + fmt.Sprintf("%d%d", index, i),
				Nic:  inter.Name,
			}
			d.DeviceSpecs = append(d.DeviceSpecs, &pluginapi.DeviceSpec{
				HostPath: "/dev/sfc_char",
				ContainerPath: "/dev/sfc_char",
				Permissions: defaultPermission,
			})
			d.DeviceSpecs = append(d.DeviceSpecs, &pluginapi.DeviceSpec{
				HostPath: "/dev/sfc_affinity",
				ContainerPath: "/dev/sfc_affinity",
				Permissions: defaultPermission,
			})
			d.DeviceSpecs = append(d.DeviceSpecs, &pluginapi.DeviceSpec{
				HostPath: "/dev/onload",
				ContainerPath: "/dev/onload",
				Permissions: defaultPermission,
			})
			d.DeviceSpecs = append(d.DeviceSpecs, &pluginapi.DeviceSpec{
				HostPath: "/dev/onload_epoll",
				ContainerPath: "/dev/onload_epoll",
				Permissions: defaultPermission,
			})
			d.ID = fmt.Sprintf("%x", h.Sum(nil))
			devices = append(devices, d)
		}
		var deviceIDs []string
		for _, device := range devices {
			deviceIDs = append(deviceIDs, device.ID)
		}
		glog.V(10).Info("update net-attach-def")
		if err := resource.AddDeviceIDs(dps.resource.Name(), inter.Name, deviceIDs, dps.k8sclient); err != nil {
			return nil, err
		}
		glog.V(10).Info("update net-attach-def succ")
	}

	dps.firstRunDiscover = false
	return devices, nil
}

func (vsfc *vSfcPluginServer) TryAllocate(req []*pluginapi.ContainerAllocateRequest) (resp []*pluginapi.ContainerAllocateResponse, err error) {
	for _, containerRequest := range req {
		containerAllocateResponse := new(pluginapi.ContainerAllocateResponse)
		// Add all requested devices to to response.
		for _, id := range containerRequest.DevicesIDs {
			device, ok := vsfc.devices[id]
			if !ok {
				return nil, fmt.Errorf("requested device does not exist %q", id)
			}
			if device.Health != pluginapi.Healthy {
				return nil, fmt.Errorf("requested device is not healthy %q", id)
			}

			glog.Infof("alloc name:%s id:%s", device.Name, device.ID)

			containerAllocateResponse.Devices = append(containerAllocateResponse.Devices, device.DeviceSpecs...)
			containerAllocateResponse.Mounts = append(containerAllocateResponse.Mounts, device.Mounts...)

		}
		resp = append(resp, containerAllocateResponse)
	}

	return resp, nil
}

func (dps *vSfcPluginServer) GetPreferredAllocation(context.Context, *pluginapi.PreferredAllocationRequest) (*pluginapi.PreferredAllocationResponse, error) {
	// TODO: Assign a specific interface to pod
	return &pluginapi.PreferredAllocationResponse{}, nil
}

type memVSfcPluginServer struct {
	vSfcPluginServer
	allocAddr    string
}

func NewMemVSfcPluginServer(allocAddr, resource string) pluginapi.DevicePluginServer {
	server := &memVSfcPluginServer{
		allocAddr: allocAddr,
	}
	server.impl = server
	server.devices = make(map[string]MountDevice)
	server.resource = types.Resource(resource)
	server.firstRunDiscover = true
	server.k8sclient, _ = k8s.InClusterK8sClient()
	return server
}

func (dps *memVSfcPluginServer) RefreshDevices() ([]MountDevice, error) {
	devices, _ := dps.vSfcPluginServer.RefreshDevices()
	for _, device := range devices {
		device.Mounts = append(device.Mounts, &pluginapi.Mount{
			HostPath: sharedMemory + device.Name,
			ContainerPath: viContainerMountPath,
			ReadOnly: false,
		})
	}
	return devices, nil
}

func (dps *memVSfcPluginServer) TryAllocate(req []*pluginapi.ContainerAllocateRequest) (resp []*pluginapi.ContainerAllocateResponse, err error) {
	var client efvi.EfviServiceClient
	conn, err := grpc.Dial(dps.allocAddr, grpc.WithInsecure(), grpc.WithBlock())
	if err != nil {
		glog.Fatal("dial to allocate server failed: ", err)
		return nil, fmt.Errorf("requested allocate server is unreachable")
	}
	defer conn.Close()
	client = efvi.NewEfviServiceClient(conn)
	for _, containerRequest := range req {
		containerAllocateResponse := new(pluginapi.ContainerAllocateResponse)
		// Add all requested devices to to response.
		for _, id := range containerRequest.DevicesIDs {
			device, ok := dps.devices[id]
			if !ok {
				return nil, fmt.Errorf("requested device does not exist %q", id)
			}
			if device.Health != pluginapi.Healthy {
				return nil, fmt.Errorf("requested device is not healthy %q", id)
			}

			viResource, err := client.ApplyVirtualDevice(context.Background(),
				&efvi.ApplyRequest{
					Interface:  device.Nic,
					Name:       device.Name,
				},
			)
			if err != nil {
				return nil, fmt.Errorf("requested device apply failed err: %v", err)
			}
			if viResource.Code != 0 {
				return nil, fmt.Errorf("requested device apply error: %q", viResource.Message)
			}
			glog.Info("succ path:", viResource.ViPath)
			if viResource.QueuePath != "" {
				glog.Info("using hugepages")
				containerAllocateResponse.Mounts = append(containerAllocateResponse.Mounts, &pluginapi.Mount{
					HostPath: hugepages + device.Name,
					ContainerPath: viHugePageQueueMountPath,
					ReadOnly: false,
				})
			}

			containerAllocateResponse.Devices = append(containerAllocateResponse.Devices, device.DeviceSpecs...)
			containerAllocateResponse.Mounts = append(containerAllocateResponse.Mounts, device.Mounts...)

		}
		resp = append(resp, containerAllocateResponse)
	}
	return resp, nil
}

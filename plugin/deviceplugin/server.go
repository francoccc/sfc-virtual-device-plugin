package deviceplugin

import (
	"context"
	"crypto/sha1"
	"fmt"
	"regexp"
	"sfc-virtual-device-plugin/plugin/util"
	"strconv"
	"strings"
	"sync"
	"time"

	"sfc-virtual-device-plugin/efvi"

	"github.com/golang/glog"
	"google.golang.org/grpc"
	pluginapi "k8s.io/kubelet/pkg/apis/deviceplugin/v1beta1"
)


const (
	vNicPerNic = 8

	regExpSFC    = "(?m)[\r\n]+^.*SFC[6-9].*$"
	sharedMemory = "/dev/shm/"
	hugepages    = "/mnt/hugepages/"
	defaultPermission = "mrw"
	viAllocNamePrefix = "vi"
	viContainerMountPath = "/mnt/vi"
	viHugePageQueueMountPath = "/mnt/vi/huge"
)

// device wraps the v1.beta1.Device type to add context about
// the device needed by the GenericPlugin.
type MountDevice struct {
	pluginapi.Device
	name string
	nic string
	deviceSpecs []*pluginapi.DeviceSpec
	mounts      []*pluginapi.Mount
}

type vSFCPluginServer struct {
	mu         sync.Mutex
	devices    map[string]MountDevice
	allocAddr  string
}

func NewVSFCPluginServer(resource, pluginDir, allocServiceAddr string) Plugin {
	server := &vSFCPluginServer {
		devices:   make(map[string]MountDevice),
		allocAddr: allocServiceAddr,
	}
	return NewVSFCPlugin(resource, pluginDir, server)
}

func (vsfc *vSFCPluginServer) discoverSolarflareResources() ([]MountDevice, error) {
	var devices []MountDevice
	out, err := util.ExecCommandAfterSSH("lshw", "-short", "-class", "network")
	if err != nil {
		return nil, fmt.Errorf("Error while discovering: %v", err)
	}
	re := regexp.MustCompile(regExpSFC)
	sfcNICs := re.FindAllString(out.String(), -1)
	for index, nic := range sfcNICs {
		glog.V(2).Info("logical name: ", strings.Fields(nic)[1])
		for i := 1; i <= vNicPerNic; i++ {
			h := sha1.New()
			h.Write([]byte(nic))
			h.Write([]byte(strconv.FormatUint(uint64(i), 10)))
			d := MountDevice {
				Device: pluginapi.Device {
					Health: pluginapi.Healthy,
				},
				name: viAllocNamePrefix + fmt.Sprintf("%d%d", index, i),
				nic: strings.Fields(nic)[1],
			}
			d.deviceSpecs = append(d.deviceSpecs, &pluginapi.DeviceSpec {
				HostPath: "/dev/sfc_char",
				ContainerPath: "/dev/sfc_char",
				Permissions: defaultPermission,
			})
			d.deviceSpecs = append(d.deviceSpecs, &pluginapi.DeviceSpec {
				HostPath: "/dev/sfc_affinity",
				ContainerPath: "/dev/sfc_affinity",
				Permissions: defaultPermission,
			})
			d.mounts = append(d.mounts, &pluginapi.Mount {
				HostPath: sharedMemory + d.name,
				ContainerPath: viContainerMountPath,
				ReadOnly: false,
			})
			d.ID = fmt.Sprintf("%x", h.Sum(nil))
			devices = append(devices, d)
		}
	}
	return devices, nil
}

func (vsfc *vSFCPluginServer) refreshDevices() (bool, error) {
	devices, err := vsfc.discoverSolarflareResources()
	if err != nil {
		return false, fmt.Errorf("failed to discover devices: %v", err)
	}

	vsfc.mu.Lock()
	defer vsfc.mu.Unlock()

	old := vsfc.devices
	vsfc.devices = make(map[string]MountDevice)

	var equal bool = true
	// Add the new devices to the map and check
	// if they were in the old map.
	for _, d := range devices {
		vsfc.devices[d.ID] = d
		if _, ok := old[d.ID]; !ok {
			equal = false
		}
	}
	if !equal {
		glog.Info("update new devices count: ", len(vsfc.devices))
		return false, nil
	}

	// Check if devices were removed.
	for k := range old {
		if _, ok := vsfc.devices[k]; !ok {
			glog.Warning("found old device removed: ", k)
			return false, nil
		}
	}
	return true, nil
}

// GetDeviceState always returns healthy.
func (vsfc *vSFCPluginServer) GetDeviceState(_ string) string {
	return pluginapi.Healthy
}

// Allocate assigns generic devices to a Pod.
func (vsfc *vSFCPluginServer) Allocate(_ context.Context, req *pluginapi.AllocateRequest) (*pluginapi.AllocateResponse, error) {
	vsfc.mu.Lock()
	defer vsfc.mu.Unlock()
	res := &pluginapi.AllocateResponse {
		ContainerResponses: make([]*pluginapi.ContainerAllocateResponse, 0, len(req.ContainerRequests)),
	}
	conn, err := grpc.Dial(vsfc.allocAddr, grpc.WithInsecure(), grpc.WithBlock())
	if err != nil {
		glog.Fatal("dial to allocate server failed: ", err)
		return nil, fmt.Errorf("requested allocate server is unreachable")
	}
	defer conn.Close()
	client := efvi.NewEfviServiceClient(conn)
	for _, r := range req.ContainerRequests {
		resp := new(pluginapi.ContainerAllocateResponse)
		// Add all requested devices to to response.
		for _, id := range r.DevicesIDs {
			d, ok := vsfc.devices[id]
			if !ok {
				return nil, fmt.Errorf("requested device does not exist %q", id)
			}
			viResource, err := client.ApplyVirtualDevice(context.Background(),
				&efvi.ApplyRequest {
					Interface:  d.nic,
					Name: d.name,
			})
			if err != nil {
				return nil, fmt.Errorf("requested device apply failed err: %v", err)
			}
			if viResource.Code != 0 {
				return nil, fmt.Errorf("requested device apply error: %q", viResource.Message)
			}
			glog.Info("succ path:", viResource.ViPath)
			if d.Health != pluginapi.Healthy {
				return nil, fmt.Errorf("requested device is not healthy %q", id)
			}
			resp.Devices = append(resp.Devices, d.deviceSpecs...)
			resp.Mounts = append(resp.Mounts, d.mounts...)
			if viResource.QueuePath != "" {
				glog.Info("using hugepages")
				resp.Mounts = append(resp.Mounts, &pluginapi.Mount {
					HostPath: hugepages + d.name,
					ContainerPath: viHugePageQueueMountPath,
					ReadOnly: false,
				})
			}
		}
		res.ContainerResponses = append(res.ContainerResponses, resp)
	}

	return res, nil
}

// GetDevicePluginOptions always returns an empty response.
func (vsfc *vSFCPluginServer) GetDevicePluginOptions(_ context.Context, _ *pluginapi.Empty) (*pluginapi.DevicePluginOptions, error) {
	return &pluginapi.DevicePluginOptions{}, nil
}

// ListAndWatch lists all devices and then refreshes every deviceCheckInterval.
func (vsfc *vSFCPluginServer) ListAndWatch(_ *pluginapi.Empty, stream pluginapi.DevicePlugin_ListAndWatchServer) error {
	glog.Info("kubelet starting listwatch")
	if _, err := vsfc.refreshDevices(); err != nil {
		return err
	}
	ok := false
	var err error
	for {
		if !ok {
			res := new(pluginapi.ListAndWatchResponse)
			for _, dev := range vsfc.devices {
				res.Devices = append(res.Devices, &pluginapi.Device{ID: dev.ID, Health: dev.Health})
			}
			if err := stream.Send(res); err != nil {
				return err
			}
		}
		<-time.After(deviceCheckInterval)
		ok, err = vsfc.refreshDevices()
		if err != nil {
			return err
		}
	}
}

// PreStartContainer always returns an empty response.
func (vsfc *vSFCPluginServer) PreStartContainer(_ context.Context, _ *pluginapi.PreStartContainerRequest) (*pluginapi.PreStartContainerResponse, error) {
	return &pluginapi.PreStartContainerResponse{}, nil
}

// GetPreferredAllocation always returns an empty response.
func (vsfc *vSFCPluginServer) GetPreferredAllocation(context.Context, *pluginapi.PreferredAllocationRequest) (*pluginapi.PreferredAllocationResponse, error) {
	return &pluginapi.PreferredAllocationResponse{}, nil
}

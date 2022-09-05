package deviceplugin

import (
	"context"
	"crypto/sha1"
	"encoding/json"
	"fmt"
	"strconv"
	"sync"
	"time"

	"sfc-virtual-device-plugin/efvi"
	"sfc-virtual-device-plugin/plugin/util"

	"github.com/golang/glog"
	"google.golang.org/grpc"

	// metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"

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

// device wraps the v1.beta1.Device type to add context about
// the device needed by the GenericPlugin.
type MountDevice struct {
	pluginapi.Device
	name string
	nic string
	deviceSpecs []*pluginapi.DeviceSpec
	mounts      []*pluginapi.Mount
}

type VSFCPluginServer interface {
	pluginapi.DevicePluginServer

	SetSkipAlloc(bool)
}

type vSFCPluginServer struct {
	mu         sync.Mutex
	devices    map[string]MountDevice
	allocAddr  string
	skipAlloc  bool
	firstRunDiscover bool

	// clientset kubernetes.Interface
}

func NewDevicePluginServer(allocServiceAddr string) pluginapi.DevicePluginServer {
	// config, _ := rest.InClusterConfig()
	// clientset, _ := kubernetes.NewForConfig(config)

	return &vSFCPluginServer {
		devices:   make(map[string]MountDevice),
		allocAddr: allocServiceAddr,
		skipAlloc: false,
		firstRunDiscover: true,
		// clientset: clientset,
	}
}

func (vsfc *vSFCPluginServer) SetSkipAlloc(skipAlloc bool) {
	vsfc.skipAlloc = skipAlloc
}

func (vsfc *vSFCPluginServer) discoverSolarflareResources() ([]MountDevice, error) {
	var devices []MountDevice

	for index, nic := range util.FindSolarflareNIC() {
		if vsfc.firstRunDiscover {
			glog.Info("logical name: ", nic.Name)
		}
		for i := 1; i <= vNicPerNic; i++ {
			b, err := json.Marshal(nic)
			if err != nil {
				glog.Errorf("failed to marshal nic object: %v", err)
				continue
			}
			h := sha1.New()
			h.Write(b)
			h.Write([]byte(strconv.FormatUint(uint64(i), 10)))
			h.Write([]byte(strconv.FormatBool(vsfc.skipAlloc)))
			d := MountDevice {
				Device: pluginapi.Device {
					Health: pluginapi.Healthy,
				},
				name: viAllocNamePrefix + fmt.Sprintf("%d%d", index, i),
				nic: nic.Name,
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
			if !vsfc.skipAlloc {
				d.mounts = append(d.mounts, &pluginapi.Mount {
					HostPath: sharedMemory + d.name,
					ContainerPath: viContainerMountPath,
					ReadOnly: false,
				})
			}
			d.ID = fmt.Sprintf("%x", h.Sum(nil))
			devices = append(devices, d)
		}
	}
	vsfc.firstRunDiscover = false
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

	var client efvi.EfviServiceClient
	if !vsfc.skipAlloc {
		conn, err := grpc.Dial(vsfc.allocAddr, grpc.WithInsecure(), grpc.WithBlock())
		if err != nil {
			glog.Fatal("dial to allocate server failed: ", err)
			return nil, fmt.Errorf("requested allocate server is unreachable")
		}
		defer conn.Close()
		client = efvi.NewEfviServiceClient(conn)
	}

	for _, r := range req.ContainerRequests {
		resp := new(pluginapi.ContainerAllocateResponse)
		// Add all requested devices to to response.
		for _, id := range r.DevicesIDs {
			d, ok := vsfc.devices[id]
			if !ok {
				return nil, fmt.Errorf("requested device does not exist %q", id)
			}
			if d.Health != pluginapi.Healthy {
				return nil, fmt.Errorf("requested device is not healthy %q", id)
			}

			if !vsfc.skipAlloc {
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
				if viResource.QueuePath != "" {
					glog.Info("using hugepages")
					resp.Mounts = append(resp.Mounts, &pluginapi.Mount {
						HostPath: hugepages + d.name,
						ContainerPath: viHugePageQueueMountPath,
						ReadOnly: false,
					})
				}
			}


			resp.Devices = append(resp.Devices, d.deviceSpecs...)
			resp.Mounts = append(resp.Mounts, d.mounts...)

		}
		res.ContainerResponses = append(res.ContainerResponses, resp)
	}

	// pod, err := vsfc.clientset.CoreV1().Pods("default").Get(context.TODO(), "debian", v1.GetOptions{})
	// if err != nil {
	// 	glog.Info("pod:", pod.Name)
	// } else {
	// 	glog.Info("pod err:", err)
	// }
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

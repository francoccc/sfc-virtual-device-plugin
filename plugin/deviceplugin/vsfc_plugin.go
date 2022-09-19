package deviceplugin

import (
	"context"
	"crypto/sha1"
	"fmt"
	"net"
	"os"
	"strconv"
	"time"

	"sfc-virtual-device-plugin/efvi"
	"sfc-virtual-device-plugin/plugin/config"
	"sfc-virtual-device-plugin/plugin/util"

	"github.com/golang/glog"
	"github.com/oklog/run"
	"google.golang.org/grpc"
	pluginapi "k8s.io/kubelet/pkg/apis/deviceplugin/v1beta1"
)

const (
	socketPrefix        = "vsfc"
	socketCheckInterval = 1 * time.Second

	vNicPerNic = 8
	sharedMemory = "/dev/shm/"
	hugepages    = "/mnt/hugepages/"
	defaultPermission = "mrw"
	viAllocNamePrefix = "vi"
	viContainerMountPath = "/mnt/vi"
	viHugePageQueueMountPath = "/mnt/vi/huge"
)

type PluginImpl interface {
	CleanUp() error
	RunOnce(context.Context) error
}

type vSfcPlugin struct {
	PluginBase
	pluginapi.DevicePluginServer
	grpcServer *grpc.Server

	// metrics?
}

type MemVSfcPlugin struct {
	vSfcPlugin
}

func NewMemVSfcPlugin(resource, pluginDir string) PluginImpl {
	// TODO: not safe
	var inter *config.Interface
	for _, configed := range config.Conf().Interfaces {
		if configed.Resource == resource {
			inter = &configed
		}
	}
	if inter == nil {
		panic("none interface")
	}
	if len(inter.AllocServiceAddr) == 0 {
		panic("alloc addr not set")
	}
	dps := NewMemVSfcPluginServer(inter.AllocServiceAddr, resource)
	return &MemVSfcPlugin{
		vSfcPlugin: vSfcPlugin{
			DevicePluginServer: dps,
			PluginBase: PluginBase{
				resource: resource,
				pluginDir: pluginDir,
				socket: MakeUnixSockPath(pluginDir, resource),
			},
		},
	}
}

func NewPlugin(resource, pluginDir string) PluginImpl {
	dps := NewVSfcPluginServer(resource)
	return &vSfcPlugin{
		DevicePluginServer: dps,
		PluginBase: PluginBase{
			resource: resource,
			pluginDir: pluginDir,
			socket: MakeUnixSockPath(pluginDir, resource),
		},
	}
}

// plugin RunOnce
func (vsfc *vSfcPlugin) RunOnce(ctx context.Context) error {

	g := run.Group{}
	{
		// Run the gRPC server.
		vsfc.grpcServer = grpc.NewServer()
		pluginapi.RegisterDevicePluginServer(vsfc.grpcServer, vsfc.DevicePluginServer)
		l, err := net.Listen("unix", vsfc.socket)
		if err != nil {
			return fmt.Errorf("failed to listen on Unix socket %q: %v", vsfc.socket, err)
		}
		glog.Info("listening on Unix socket:", vsfc.socket)

		g.Add(func() error {
			glog.Info("starting gRPC server")
			if err := vsfc.grpcServer.Serve(l); err != nil {
				return fmt.Errorf("gRPC server exited unexpectedly: %v", err)
			}
			return nil
		}, func(error) {
			vsfc.grpcServer.Stop()
		})
	}

	{
		// Register the plugin with the kubelet.
		ctx, cancel := context.WithCancel(ctx)
		g.Add(func() error{
			glog.Info("waiting for the gRPC server to be ready")
			c, err := grpc.DialContext(ctx, vsfc.socket, grpc.WithInsecure(), grpc.WithBlock(),
				grpc.WithContextDialer(func(ctx context.Context, addr string) (net.Conn, error) {
					return (&net.Dialer{}).DialContext(ctx, "unix", addr)
				}),
			)
			if err != nil {
				return fmt.Errorf("failed to create connection to local gRPC server: %v", err)
			}
			if err := c.Close(); err != nil {
				return fmt.Errorf("failed to close connection to local gRPC server: %v", err)
			}
			glog.Info("the gRPC server is ready")
			if err := vsfc.registerWithKubelet(); err != nil {
				return fmt.Errorf("failed to register with kubelet: %v", err)
			}
			<-ctx.Done()
			return nil
		}, func(error) {
			cancel()
		})
	}

	{
		// Watch the socket.
		t := time.NewTicker(socketCheckInterval)
		ctx, cancel := context.WithCancel(ctx)
		g.Add(func() error {
			defer t.Stop()
			for {
				select {
				case <-t.C:
					if _, err := os.Lstat(vsfc.socket); err != nil {
						return fmt.Errorf("failed to stat plugin socket", vsfc.socket, err)
					}
				case <-ctx.Done():
					return nil
				}
			}
		}, func(error) {
			cancel()
		})
	}
	return g.Run()
}

// plugin CleanUp
func (p *vSfcPlugin) CleanUp() error {
	glog.Info("Try clean up vsfc plugin")
	if err := os.Remove(p.socket); err != nil && !os.IsNotExist(err) {
		return fmt.Errorf("failed to remove socket: %v", err)
	}
	glog.Info("Cleaned up vsfc plugin")
	return nil
}

type PluginServerImpl interface {
	RefreshDevices() ([]MountDevice, error)
	TryAllocate([]*pluginapi.ContainerAllocateRequest) ([]*pluginapi.ContainerAllocateResponse, error)
}

type vSfcPluginServer struct {
	PluginServerBase
	firstRunDiscover bool
	resource         string
}

func NewVSfcPluginServer(resource string) pluginapi.DevicePluginServer {
	server := &vSfcPluginServer{
		resource: resource,
		firstRunDiscover: true,
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
			if dps.resource == configed.Resource && inter.Name == configed.Name {
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
	server.resource = resource
	server.firstRunDiscover = true
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

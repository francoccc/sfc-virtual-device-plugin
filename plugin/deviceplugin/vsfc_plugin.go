package deviceplugin

import (
	"context"
	"fmt"
	"net"
	"os"
	"time"

	k8s "sfc-virtual-device-plugin/plugin/client"
	"sfc-virtual-device-plugin/plugin/config"
	"sfc-virtual-device-plugin/plugin/resource/types"

	"github.com/golang/glog"
	"github.com/oklog/run"
	"google.golang.org/grpc"
	pluginapi "k8s.io/kubelet/pkg/apis/deviceplugin/v1beta1"
)

const (
	socketPrefix        = "vsfc"
	socketCheckInterval = time.Duration(1) * time.Second
	netAttachdefCheckInterval = time.Duration(1) * time.Second
)

type vSfcPlugin struct {
	PluginBase
	pluginapi.DevicePluginServer
	grpcServer *grpc.Server

	// metrics?
}

type MemVSfcPlugin struct {
	vSfcPlugin
}

func NewMemVSfcPlugin(resource, pluginDir string) (PluginImpl, error) {
	allocServiceAddr := getAllocAddr(resource)
	if len(allocServiceAddr) <= 0 {
		return nil, fmt.Errorf("invalid allocServiceAddr")
	}
	dps := NewMemVSfcPluginServer(allocServiceAddr, resource)
	return &MemVSfcPlugin{
		vSfcPlugin: vSfcPlugin{
			DevicePluginServer: dps,
			PluginBase: PluginBase{
				resource: types.Resource(resource),
				pluginDir: pluginDir,
				socket: MakeUnixSockPath(pluginDir, resource),
			},
		},
	}, nil
}

func NewPlugin(resource, pluginDir string) (PluginImpl, error) {
	dps := NewVSfcPluginServer(resource)
	return &vSfcPlugin{
		DevicePluginServer: dps,
		PluginBase: PluginBase{
			resource: types.Resource(resource),
			pluginDir: pluginDir,
			socket: MakeUnixSockPath(pluginDir, resource),
		},
	}, nil
}

// plugin RunOnce
func (vsfc *vSfcPlugin) RunOnce(ctx context.Context, k8sclient *k8s.ClientInfo) error {

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

	{
		// Watch the net-attach-def(CRD), if classify by user
		if !types.DefaultResource[vsfc.resource] {
			glog.Info("prepare to watch net-attach-def resource: ", vsfc.resource.Name())
			t := time.NewTicker(netAttachdefCheckInterval)
			ctx, cancel := context.WithCancel(ctx)
			g.Add(func() error {
				defer t.Stop()
				if k8sclient == nil {
					return fmt.Errorf("no k8sclient")
				}
				// Ensure reference
				k8sclient.AddNetAttachDefByResource(string(vsfc.resource), vsfc.resource.Name())
				glog.Info("start watching net-attach-def: ", vsfc.resource.Name())
				for {
					select {
					case <-t.C:
						if nad, _ := k8sclient.GetNetAttachDef("default", vsfc.resource.Name()); nad == nil {
							// TODO: if NetError happen
							return fmt.Errorf("no netattachdefinition")
						}
					case <-ctx.Done():
						glog.Info("watching net-attach-def end")
						return nil
					}
				}
			}, func(error) {
				cancel()
			})
		}
	}
	return g.Run()
}

// plugin CleanUp
func (vsfc *vSfcPlugin) CleanUp() error {
	glog.Info("Try clean up vsfc plugin")
	if err := os.Remove(vsfc.socket); err != nil && !os.IsNotExist(err) {
		return fmt.Errorf("failed to remove socket: %v", err)
	}
	glog.Info("Cleaned up vsfc plugin")
	return nil
}

func getAllocAddr(resource string) string {
	var inter *config.Interface
	for _, configed := range config.Conf().Interfaces {
		if configed.Resource == resource {
			inter = &configed
		}
	}
	if inter == nil {
		return ""
	}
	return inter.AllocServiceAddr
}

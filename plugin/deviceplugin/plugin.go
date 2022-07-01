/*
 * @Descripition: vsfc plugin.go
 * @Author: Franco Chen
 * @Date: 2022-06-27 11:37:55
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-06-28 16:45:08
 */
package deviceplugin

import (
	"context"
	"encoding/base64"
	"fmt"
	"net"
	"os"
	"path/filepath"
	"sync"
	"time"

	"github.com/golang/glog"
	"github.com/oklog/run"
	"google.golang.org/grpc"
	pluginapi "k8s.io/kubelet/pkg/apis/deviceplugin/v1beta1"
)

const (
	socketPrefix        = "vsfc"
	regExpSFC           = "(?m)[\r\n]+^.*SFC[6-9].*$"
	socketCheckInterval = 1 * time.Second
	restartInterval     = 5 * time.Second
	deviceCheckInterval = 5 * time.Second
)

var mutex sync.Mutex

type Plugin interface {
	pluginapi.DevicePluginServer
	Run(context.Context) error
}

type vSFCPlugin struct {
	pluginapi.DevicePluginServer
	resource   string
	pluginDir  string
	socket     string
	grpcServer *grpc.Server
	// metrics?
}

func NewVSFCPlugin(resource, pluginDir string, dps pluginapi.DevicePluginServer) Plugin {
	return &vSFCPlugin {
		DevicePluginServer: dps,
		resource:           resource,
		pluginDir:          pluginDir,
		socket:             MakeUnixSockPath(pluginDir, resource),
	}
}

func MakeUnixSockPath(pluginDir, resource string) string {
	return filepath.Join(
		pluginDir,
		fmt.Sprintf(
			"%s-%s-%d.sock", 
			socketPrefix, 
			base64.StdEncoding.EncodeToString([]byte(resource)), 
			time.Now().Unix(),
		),
	)
}

func (vsfc *vSFCPlugin) Run(ctx context.Context) error {
	Outer:
		for {
			select {
			case <-ctx.Done():
				break Outer
			default:
				if err := vsfc.runOnce(ctx); err != nil {
					glog.Info("encountered error while running plugin; trying again in 5 seconds", " err: ", err)
					select {
					case <-ctx.Done():
						break Outer
					case <-time.After(restartInterval):
						glog.Info("Restarting...")
					}
				}
			}
		}
		return vsfc.cleanUp()
}

// plugin RunOnce
func (vsfc *vSFCPlugin) runOnce(ctx context.Context) error {
	
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

func (vsfc *vSFCPlugin) registerWithKubelet() error {
	glog.Info("registering plugin with kubelet")
	conn, err := grpc.Dial(
		filepath.Join(vsfc.pluginDir, filepath.Base(pluginapi.KubeletSocket)), 
		grpc.WithInsecure(),
		grpc.WithBlock(),
		grpc.WithDialer(func(addr string, timeout time.Duration) (net.Conn, error) {
			return net.DialTimeout("unix", addr, timeout)
		}),
	)
	if err != nil {
		return fmt.Errorf("failed to connect to kubelet: %v", err)
	}
	defer conn.Close()

	client := pluginapi.NewRegistrationClient(conn)
	request := &pluginapi.RegisterRequest{
		Version:      pluginapi.Version,
		Endpoint:     filepath.Base(vsfc.socket),
		ResourceName: vsfc.resource,
	}
	if _, err = client.Register(context.Background(), request); err != nil {
		return fmt.Errorf("failed to register plugin with kubelet service: %v", err)
	}
	glog.Info("registered plugin with kubelet succ")
	return nil
}

func (p *vSFCPlugin) cleanUp() error {
	glog.Info("Try clean up vsfc plugin")
	if err := os.Remove(p.socket); err != nil && !os.IsNotExist(err) {
		return fmt.Errorf("failed to remove socket: %v", err)
	}
	glog.Info("Cleaned up vsfc plugin")
	return nil
}
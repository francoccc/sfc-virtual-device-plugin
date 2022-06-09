/*
 * @Descripition:
 * @Author: Franco Chen
 * @Date: 2022-06-06 17:30:44
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-06-09 17:40:39
 */
package main

import (
	"bytes"
	"flag"
	"fmt"
	"os"
	"os/exec"
	"time"

	efvi "example.com/repo/efvi/service"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	pluginapi "k8s.io/kubernetes/pkg/kubelet/apis/deviceplugin/v1alpha"
)

type SfcNicManager struct {
	devices            map[string]*pluginapi.Device
	deviceFile        []string
}

var (
	efviClient efvi.EfviServiceClient
	addr = flag.String("addr", "localhost:8088", "efvi_allocator local address")
)



func NewSfcNicManager() (*SfcNicManager, error) {
	return &SfcNicManager{
		devices:     make(map[string]*pluginapi.Device),
		deviceFile: []string{"/dev/onload", "/dev/onload_cplane", "dev/onload_epoll", "/dev/sfc_char", "/dev/sfc_affinity"},
	}, nil
}

func ExecCommand(cmdName string, arg ...string) (bytes.Buffer, error) {
	var out bytes.Buffer
	var stderr bytes.Buffer

	cmd := exec.Command(cmdName, arg...)
	cmd.Stdout = &out
	cmd.Stderr = &stderr
	err := cmd.Run()
	if err != nil {
		fmt.Println("CMD--" + cmdName + ": " + fmt.Sprint(err) + ": " + stderr.String())
	}

	return out, err
}

func ForkProcess(cmdName string, args []string) {
	var attr = os.ProcAttr {
			Dir: ".",
			Env: os.Environ(),
			Files: []*os.File{
				os.Stdin,
				os.Stdout,
        os.Stderr,
			},
	}
	
	process, err := os.StartProcess(cmdName, args, &attr)
		if err != nil {
			fmt.Println(err.Error())
		} else {
			err = process.Release()
			if err != nil {
				fmt.Println(err.Error())
			}
		}
}

func main() {
	flag.Parse()
	fmt.Println("Start")
	
	// Set up a connection to the server.
	conn, err := grpc.Dial(*addr, grpc.WithTransportCredentials(insecure.NewCredentials()))
	
	socketName := os.Args[1]   // "sfcNic"
	
	
	go func() {
		ForkProcess("test.sh", []string{"test.sh", "1"})
	}()
	
	pluginEndpoint := fmt.Sprintf("%s-%d.sock", socketName, time.Now().Unix())
	fmt.Println(pluginEndpoint)
	time.Sleep(300 * time.Second)
  fmt.Println("Sleep Over.....")
}

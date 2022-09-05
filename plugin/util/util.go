package util

import (
	"bytes"
	"fmt"
	"os"
	"os/exec"
	"regexp"
	"strings"

	"github.com/golang/glog"
)

const (
	regExpSFC = "(?m)[\r\n]+^.*SFC[6-9].*$"
)

func ExecCommandAfterSSH(cmdName string, args ...string) (bytes.Buffer, error) {
	ssh_args := []string{"-o", "StrictHostKeyChecking=no", "127.0.0.1"}
	ssh_args = append(ssh_args, cmdName)
	ssh_args = append(ssh_args, args...)
	return ExecCommand("ssh", ssh_args...)
}

func ExecCommand(cmdName string, arg ...string) (bytes.Buffer, error) {
	var out bytes.Buffer
	var stderr bytes.Buffer

	cmd := exec.Command(cmdName, arg...)
	cmd.Stdout = &out
	cmd.Stderr = &stderr
	err := cmd.Run()
	if err != nil {
		glog.Error("CMD--", cmdName, ": ", fmt.Sprint(err), ": ", stderr.String())
	}

	return out, err
}

func ForkProcess(cmdName string, args []string) {
	var attr = os.ProcAttr{
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

type NicLshwDetail struct {
	Path  string `json:"path"`
	Name  string `json:"name"`
	Class string `json:"class"`
	Desc  string `json:"desc"`
}

func FindSolarflareNIC() []NicLshwDetail {
	var nics []NicLshwDetail
	out, err := ExecCommandAfterSSH("lshw", "-short", "-class", "network")
	if err != nil {
		glog.Error("Error while finding network card ", fmt.Sprint(err))
		return nics
	}
	re := regexp.MustCompile(regExpSFC)
	for _, nic := range re.FindAllString(out.String(), -1) {
		n := NicLshwDetail{
			Path:  strings.Fields(nic)[0],
			Name:  strings.Fields(nic)[1],
			Class: strings.Fields(nic)[2],
			Desc:  strings.Fields(nic)[3],
		}
		nics = append(nics, n)
	}
	return nics
}

package types

import (
	"strings"
)

type Resource string

const (
	VirtualSfc       Resource = "highfortfunds.com/vsfc"
	VirtualMemSfc    Resource = "highfortfunds.com/vsfc-mem"
	VirtualSfcHandle Resource = "highfortfunds.com/vsfc-handle"


	NodeResourceAnnotPrefix = "resource.highfortfunds.com/"
)

var (
	DefaultResource = map[Resource]bool{
		VirtualSfc: true,
		VirtualMemSfc: true,
		VirtualSfcHandle: true,
	}
)

func (r Resource) Name() string {
	return string(r)[strings.Index(string(r), "/") + 1 :]
}

type DeviceInfo struct {
	Name string `json:"name"`
	DeviceIDs []string `json:"deviceIDs"`
}

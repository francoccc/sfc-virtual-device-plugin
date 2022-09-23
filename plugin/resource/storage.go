/*
 * @Descripition:
 * @Author: Franco Chen
 * @Date: 2022-09-22 17:14:11
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-09-23 10:49:38
 */
package resource

import (
	"encoding/json"
	"fmt"
	"os"

	k8s "sfc-virtual-device-plugin/plugin/client"
	"sfc-virtual-device-plugin/plugin/resource/types"

	nettypes "github.com/k8snetworkplumbingwg/network-attachment-definition-client/pkg/apis/k8s.cni.cncf.io/v1"
)

func ParseDeviceInfo(raw []byte) ([]types.DeviceInfo, error) {
	var devices []types.DeviceInfo
	if err := parseDeviceInfo(raw, &devices); err != nil {
		return nil, err
	}
	return devices, nil
}

func parseDeviceInfo(raw []byte, devices *[]types.DeviceInfo) error {
	err := json.Unmarshal(raw, devices)
	return err
}

func AddDeviceIDs(netAttachDef, ifName string, deviceIDs []string, k8sclient *k8s.ClientInfo) error {
	if k8sclient == nil {
		return fmt.Errorf("empty k8s client")
	}
	nad, err := k8sclient.GetDefaultNetAttachDef(netAttachDef)
	if err != nil {
		return err
	}

	if err := addDeviceIeIDs(ifName, deviceIDs, nad); err != nil {
		return err
	}

	if _, err := k8sclient.UpdateNetAttachDef(nad); err != nil {
		return err
	}

	return nil
}

func addDeviceIeIDs(ifName string, deviceIDs []string, nad *nettypes.NetworkAttachmentDefinition) error {
	node := os.Getenv("NODE_NAME")
	if len(node) <= 0 {
		return fmt.Errorf("empty node name")
	}
	annotation := types.NodeResourceAnnotPrefix + node
	var devices []types.DeviceInfo
	if content, ok := nad.Annotations[annotation]; ok {
		if err := parseDeviceInfo([]byte(content), &devices); err != nil {
			return err
		}
		if !hasTargetDevice(ifName, devices) {
			devices = append(devices, types.DeviceInfo{
				Name:      ifName,
				DeviceIDs: deviceIDs,
			})
		} else {
			updateTargetDevice(ifName, deviceIDs, devices)
		}
	} else {
		devices = append(devices, types.DeviceInfo{
			Name:      ifName,
			DeviceIDs: deviceIDs,
		})
	}

	bytes, err := json.Marshal(devices)
	if err != nil {
		return err
	}
	nad.Annotations[annotation] = string(bytes)
	return nil
}

func hasTargetDevice(ifName string, devices []types.DeviceInfo) bool {
	for _, device := range devices {
		if device.Name == ifName {
			return true
		}
	}
	return false
}

func updateTargetDevice(ifName string, deviceIDs []string, devices []types.DeviceInfo) {
	for _, device := range devices {
		if device.Name == ifName {
			device.DeviceIDs = deviceIDs
		}
	}
}

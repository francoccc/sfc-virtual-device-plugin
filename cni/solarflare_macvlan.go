package main

import (
	"encoding/json"
	"fmt"
	"net"
	"sfc-virtual-device-plugin/plugin/util"

	"github.com/vishvananda/netlink"

	"github.com/containernetworking/cni/pkg/skel"
	"github.com/containernetworking/cni/pkg/types"
	current "github.com/containernetworking/cni/pkg/types/100"
	"github.com/containernetworking/cni/pkg/version"

	"github.com/containernetworking/plugins/pkg/ip"
	"github.com/containernetworking/plugins/pkg/ipam"
	"github.com/containernetworking/plugins/pkg/ns"
	bv "github.com/containernetworking/plugins/pkg/utils/buildversion"
	"github.com/containernetworking/plugins/pkg/utils/sysctl"
)

type NetConf struct {
	types.NetConf
	Mode string `json:"mode"`
	MTU  int    `json:"mtu"`
	Mac  string `json:"mac,omitempty"`

	RuntimeConfig struct {
		Mac string `json:"mac,omitempty"`
	} `json:"runtimeConfig,omitempty"`
}

type solarflareInterface struct {
	current.Interface
	ParentName string
}

func getDefaultRouteInterfaceName() (string, error) {
	routeToDstIP, err := netlink.RouteList(nil, netlink.FAMILY_ALL)
	if err != nil {
		return "", err
	}

	for _, v := range routeToDstIP {
		if v.Dst == nil {
			l, err := netlink.LinkByIndex(v.LinkIndex)
			if err != nil {
				return "", err
			}
			return l.Attrs().Name, nil
		}
	}

	return "", fmt.Errorf("no default route interface found")
}

func loadConf(bytes []byte, envArgs string) (*NetConf, string, error) {
	n := &NetConf{}
	if err := json.Unmarshal(bytes, n); err != nil {
		return nil, "", fmt.Errorf("failed to load conf: %v", err)
	}

	if envArgs != "" {

	}
	return n, n.CNIVersion, nil
}

func modeFromString(s string) (netlink.MacvlanMode, error) {
	switch s {
	case "", "bridge":
		return netlink.MACVLAN_MODE_BRIDGE, nil
	case "private":
		return netlink.MACVLAN_MODE_PRIVATE, nil
	case "vepa":
		return netlink.MACVLAN_MODE_VEPA, nil
	case "passthru":
		return netlink.MACVLAN_MODE_PASSTHRU, nil
	default:
		return 0, fmt.Errorf("unknown macvlan mode: %q", s)
	}
}

func modeToString(mode netlink.MacvlanMode) (string, error) {
	switch mode {
	case netlink.MACVLAN_MODE_BRIDGE:
		return "bridge", nil
	case netlink.MACVLAN_MODE_PRIVATE:
		return "private", nil
	case netlink.MACVLAN_MODE_VEPA:
		return "vepa", nil
	case netlink.MACVLAN_MODE_PASSTHRU:
		return "passthru", nil
	default:
		return "", fmt.Errorf("unknown macvlan mode: %q", mode)
	}
}

func createMacvlan(conf *NetConf, ifName string, netns ns.NetNS) (*solarflareInterface, error) {
	macvlan := &solarflareInterface{}

	mode, err := modeFromString(conf.Mode)
	if err != nil {
		return nil, err
	}

	solarflare_nics := util.FindSolarflareNIC()
	m, err := netlink.LinkByName(solarflare_nics[0].Name)
	if err != nil {
		return nil, fmt.Errorf("failed to lookup master %q: %v", solarflare_nics[0].Name, err)
	}
	macvlan.ParentName = solarflare_nics[0].Name

	// due to kernel bug we have to create with tmpName or it might
	// collide with the name on the host and error out
	tmpName, err := ip.RandomVethName()
	if err != nil {
	return nil, err
	}

	linkAttrs := netlink.LinkAttrs{
		MTU:         conf.MTU,
		Name:        tmpName,
		ParentIndex: m.Attrs().Index,
		Namespace:   netlink.NsFd(int(netns.Fd())),
	}

	// if conf.Mac != "" {
	// 	addr, err := net.ParseMAC(conf.Mac)
	// 	if err != nil {
	// 		return nil, fmt.Errorf("invalid args %v for MAC addr: %v", conf.Mac, err)
	// 	}
	// 	linkAttrs.HardwareAddr = addr
	// }

	mv := &netlink.Macvlan{
		LinkAttrs: linkAttrs,
		Mode:      mode,
	}

	if err := netlink.LinkAdd(mv); err != nil {
		return nil, fmt.Errorf("failed to create macvlan: %v", err)
	}

	err = netns.Do(func(_ ns.NetNS) error {
		err := ip.RenameLink(tmpName, ifName)
		if err != nil {
			_ = netlink.LinkDel(mv)
			return fmt.Errorf("failed to rename macvlan to %q: %v", ifName, err)
		}
		macvlan.Name = ifName

		// Re-fetch macvlan to get all properties/attributes
		contMacvlan, err := netlink.LinkByName(ifName)
		if err != nil {
			return fmt.Errorf("failed to refetch macvlan %q: %v", ifName, err)
		}
		macvlan.Mac = contMacvlan.Attrs().HardwareAddr.String()
		macvlan.Sandbox = netns.Path()

		return nil
	})
	if err != nil {
		return nil, err
	}

	return macvlan, nil
}

func getAddrsFromInterface(interfaceName string) ([]net.IPNet, error) {
	var ipNets []net.IPNet
	inter, err := net.InterfaceByName(interfaceName)
	if err != nil {
		return nil, err
	}

	addrs, err := inter.Addrs()
	if err != nil {
		return nil, err
	}
	for _, addr := range addrs {
		ipNet, isValidIpNet := addr.(*net.IPNet)
		if isValidIpNet && !ipNet.IP.IsLoopback() {
			ipNets = append(ipNets, *ipNet)
		}
	}

	return ipNets, nil
}

func getIPsOfIPAM(addrs []net.IPNet) ([]*current.IPConfig) {
	var IPs []*current.IPConfig
	for _, addr := range addrs {
		IPs = append(IPs, &current.IPConfig{
			Interface: current.Int(0),
			Address: addr,
			// TODO Gateway: ,
		})
	}
	return IPs
}

// we want to use same ip
func cmdAdd(args *skel.CmdArgs) error {
	n, cniVersion, err := loadConf(args.StdinData, args.Args)
	if err != nil {
		return err
	}
	// isLayer3 := n.IPAM.Type != ""

	netns, err := ns.GetNS(args.Netns)
	if err != nil {
		return fmt.Errorf("failed to open netns %q: %v", netns, err)
	}
	defer netns.Close()

	macvlanInterface, err := createMacvlan(n, args.IfName, netns)
	if err != nil {
		return err
	}

	// Delete link if err to avoid link leak in this ns
	defer func() {
		if err != nil {
			netns.Do(func(_ ns.NetNS) error {
				return ip.DelLinkByName(args.IfName)
			})
		}
	}()

	// Get ip addrs of the host interface
	addrs, err := getAddrsFromInterface(macvlanInterface.ParentName)
	if err != nil {
		return err
	}

	//// Assume L2 interface only
	result := &current.Result{
		CNIVersion: current.ImplementedSpecVersion,
		Interfaces: []*current.Interface{&macvlanInterface.Interface},
		IPs: getIPsOfIPAM(addrs),
	}

	if n.IPAM.Type != "" {
		// manully configure routes
		// run the IPAM plugin and get back the config to apply
		r, err := ipam.ExecAdd(n.IPAM.Type, args.StdinData)
		if err != nil {
			return err
		}

		// Invoke ipam del if err to avoid ip leak
		defer func() {
			if err != nil {
				ipam.ExecDel(n.IPAM.Type, args.StdinData)
			}
		}()

		// Convert whatever the IPAM result was into the current Result type
		ipamResult, err := current.NewResultFromResult(r)
		if err != nil {
			return err
		}

		result.Routes = ipamResult.Routes
	}

	// configure ipNet
	err = netns.Do(func(_ ns.NetNS) error {
		_, _ = sysctl.Sysctl(fmt.Sprintf("net/ipv4/conf/%s/arp_notify", args.IfName), "1")
		link, err := netlink.LinkByName(args.IfName)
		if err != nil {
			return fmt.Errorf("failed to lookup %q: %v", args.IfName, err)
		}
		// we don't resolve the IPV6 Now
		for _, ipc := range result.IPs {
			if ipc.Address.IP.To4() == nil {
				continue
			}
			addr := &netlink.Addr{IPNet: &ipc.Address, Label: ""}
			if err = netlink.AddrAdd(link, addr); err != nil {
				return fmt.Errorf("failed to add IP addr %v to %q: %v", ipc, args.IfName, err)
			}
		}

		// // TODO routes
		// for _, r := range res.Routes {
		// 	routeIsV4 := r.Dst.IP.To4() != nil
		// 	gw := r.GW
		// 	if gw == nil {
		// 		if routeIsV4 && v4gw != nil {
		// 			gw = v4gw
		// 		} else if !routeIsV4 && v6gw != nil {
		// 			gw = v6gw
		// 		}
		// 	}
		// 	route := netlink.Route{
		// 		Dst:       &r.Dst,
		// 		LinkIndex: link.Attrs().Index,
		// 		Gw:        gw,
		// 	}

		// 	if err = netlink.RouteAddEcmp(&route); err != nil {
		// 		return fmt.Errorf("failed to add route '%v via %v dev %v': %v", r.Dst, gw, ifName, err)
		// 	}
		// }

		if err := netlink.LinkSetUp(link); err != nil {
			return fmt.Errorf("failed to set %q UP: %v", args.IfName, err)
		}
		return nil
	})
	if err != nil {
		return nil
	}

	result.DNS = n.DNS

	return types.PrintResult(result, cniVersion)
}

func cmdDel(args *skel.CmdArgs) error {
	_, _, err := loadConf(args.StdinData, args.Args)
	if err != nil {
		return err
	}

	// to call ipam.ExecDel()
	if args.Netns == "" {
		return nil
	}

	// There is a netns so try to clean up. Delete can be called multiple times
	// so don't return an error if the device is already removed.
	err = ns.WithNetNSPath(args.Netns, func(_ ns.NetNS) error {
		if err := ip.DelLinkByName(args.IfName); err != nil {
			if err != ip.ErrLinkNotFound {
				return err
			}
		}
		return nil
	})

	if err != nil {
		//  if NetNs is passed down by the Cloud Orchestration Engine, or if it called multiple times
		// so don't return an error if the device is already removed.
		// https://github.com/kubernetes/kubernetes/issues/43014#issuecomment-287164444
		_, ok := err.(ns.NSPathNotExistErr)
		if ok {
			return nil
		}
		return err
	}

	return err
}

func cmdCheck(args *skel.CmdArgs) error {
	n, _, err := loadConf(args.StdinData, args.Args)
	if err != nil {
		return err
	}
	// __ := n.IPAM.Type != "" // is Layer3

	// isLayer3  ipam.ExecCheck()

	netns, err := ns.GetNS(args.Netns)
	if err != nil {
		return fmt.Errorf("failed to open netns %q: %v", netns, err)
	}
	defer netns.Close()

	var containerInterface current.Interface

	// Parse previous result.
	if n.NetConf.RawPrevResult == nil {
		return fmt.Errorf("Required prevResult missing")
	}

	if err := version.ParsePrevResult(&n.NetConf); err != nil {
		return err
	}

	result, err := current.NewResultFromResult(n.PrevResult)
	if err != nil {
		return err
	}

	var contMap current.Interface
	// Find interfaces for names whe know, macvlan device name inside container
	for _, intf := range result.Interfaces {
		if args.IfName == intf.Name {
			if args.Netns == intf.Sandbox {
				containerInterface = *intf
				continue
			}
		}
	}

	// The namespace must be the same as what was configured
	if args.Netns != contMap.Sandbox {
		return fmt.Errorf("Sandbox in prevResult %s doesn't match configured netns: %s",
			contMap.Sandbox, args.Netns)
	}


	solarflare_nics := util.FindSolarflareNIC()
	netlink, err := netlink.LinkByName(solarflare_nics[0].Name)
	if err != nil {
		return fmt.Errorf("failed to lookup interface %q: %v", solarflare_nics[0].Name, err)
	}

	if err := netns.Do(func(_ ns.NetNS) error {
		// Check interface against values found in the container
		err := validateCniContainerInterface(containerInterface, netlink.Attrs().Index, n.Mode)
		if err != nil {
			return err
		}

		err = ip.ValidateExpectedInterfaceIPs(args.IfName, result.IPs)
		if err != nil {
			return err
		}

		err = ip.ValidateExpectedRoute(result.Routes)
		if err != nil {
			return err
		}
		return nil
	}); err != nil {
		return err
	}

	return nil
}

func validateCniContainerInterface(intf current.Interface, parentIndex int, modeExpected string) error {
	var link netlink.Link
	var err error
	if intf.Name == "" {
		return fmt.Errorf("containter Interface name misssing in prevResult: %v", intf.Name)
	}
	link, err = netlink.LinkByName(intf.Name)
	if err != nil {
		return fmt.Errorf("containter Interface name in prevResult: %s not found", intf.Name)
	}
	if intf.Sandbox == "" {
		return fmt.Errorf("container interface %s should not be in host namespace", link.Attrs().Name)
	}
	macv, isMacvlan := link.(*netlink.Macvlan)
	if !isMacvlan {
		return fmt.Errorf("container interface %s not of type macvlan", link.Attrs().Name)
	}
	mode, err := modeFromString(modeExpected)
	if macv.Mode != mode {
		currString, err := modeToString(macv.Mode)
		if err != nil {
			return err
		}
		confString, err := modeToString(mode)
		if err != nil {
			return err
		}
		return fmt.Errorf("containter macvlan mode %s does not match expected value: %s", currString, confString)
	}
	return nil
}

func main() {
	skel.PluginMain(cmdAdd, cmdCheck, cmdDel, version.All, bv.BuildString("sfcmacvlan"))
}

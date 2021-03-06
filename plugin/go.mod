module sfc-virtual-device-plugin

go 1.16

replace k8s.io/api => k8s.io/api v0.19.7

replace k8s.io/apiextensions-apiserver => k8s.io/apiextensions-apiserver v0.19.7

replace k8s.io/apimachinery => k8s.io/apimachinery v0.19.8-rc.0

replace k8s.io/apiserver => k8s.io/apiserver v0.19.7

replace k8s.io/cli-runtime => k8s.io/cli-runtime v0.19.7

replace k8s.io/client-go => k8s.io/client-go v0.19.7

replace k8s.io/cloud-provider => k8s.io/cloud-provider v0.19.7

replace k8s.io/cluster-bootstrap => k8s.io/cluster-bootstrap v0.19.7

replace k8s.io/code-generator => k8s.io/code-generator v0.19.9-rc.0

replace k8s.io/component-base => k8s.io/component-base v0.19.7

replace k8s.io/component-helpers => k8s.io/component-helpers v0.20.5

replace k8s.io/controller-manager => k8s.io/controller-manager v0.19.17-rc.0

replace k8s.io/cri-api => k8s.io/cri-api v0.19.9-rc.0

replace k8s.io/csi-translation-lib => k8s.io/csi-translation-lib v0.19.7

replace k8s.io/kube-aggregator => k8s.io/kube-aggregator v0.19.7

replace k8s.io/kube-controller-manager => k8s.io/kube-controller-manager v0.19.7

replace k8s.io/kube-proxy => k8s.io/kube-proxy v0.19.7

replace k8s.io/kube-scheduler => k8s.io/kube-scheduler v0.19.7

replace k8s.io/kubectl => k8s.io/kubectl v0.19.7

replace k8s.io/kubelet => k8s.io/kubelet v0.19.7

replace k8s.io/legacy-cloud-providers => k8s.io/legacy-cloud-providers v0.19.7

replace k8s.io/metrics => k8s.io/metrics v0.19.7

replace k8s.io/mount-utils => k8s.io/mount-utils v0.20.7-rc.0

replace k8s.io/pod-security-admission => k8s.io/pod-security-admission v0.24.0

replace k8s.io/sample-apiserver => k8s.io/sample-apiserver v0.19.7

replace k8s.io/sample-cli-plugin => k8s.io/sample-cli-plugin v0.19.7

replace k8s.io/sample-controller => k8s.io/sample-controller v0.19.7

require (
	github.com/gogo/protobuf v1.3.2 // indirect
	github.com/golang/glog v0.0.0-20160126235308-23def4e6c14b
	github.com/oklog/run v1.1.0
	golang.org/x/net v0.0.0-20220127200216-cd36cc0744dd // indirect
	golang.org/x/sys v0.0.0-20220209214540-3681064d5158 // indirect
	google.golang.org/genproto v0.0.0-20220107163113-42d7afdf6368 // indirect
	google.golang.org/grpc v1.40.0
	google.golang.org/protobuf v1.27.1
	k8s.io/kubelet v0.0.0
)

replace k8s.io/node-api => k8s.io/node-api v0.16.15

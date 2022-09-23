package config


type Config struct {
	PluginDir string `yaml:"pluginDir,omitempty"`
	Interfaces []Interface `yaml:"interfaces"`
}

type Interface struct {
	Name     string  `yaml:"name"`
	Resource string  `yaml:"resource"`
	Parent   string  `yaml:"parent,omitempty"`
	Splits  int `yaml:"splits,omitempty"`
	AllocServiceAddr string `yaml:"allocServiceAddr,omitempty"`
}

type NetAttachDefConf struct {
	CNIVersion string `json:"cniVersion,omitempty"`
	Name string `json:"name"`
	Plugins []Plugin `json:"plugins"`
}

type Plugin struct {
	Type string `json:"type"`
	Mode string `json:"mode"`
}

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

package config

import (
	"bytes"
	"context"
	"crypto/md5"
	"fmt"
	"io/ioutil"
	"os"
	"sync"
	"time"

	"github.com/golang/glog"
	yaml "gopkg.in/yaml.v3"
)

const (
	filename = "/etc/solarflare/nic.conf"
	scanInterval = time.Duration(10) * time.Second
)

var (
	globalLock sync.Mutex
	started = false
	instance config
)

type config struct {
	guard sync.RWMutex

	conf *Config
	sum  []byte
	subscribers map[string]chan struct{}
}

func (c *config) scan() error {
	file, err := os.OpenFile(filename, os.O_RDONLY, 0)
	if err != nil {
		return fmt.Errorf("config opening err: %v", err)
	}
	defer file.Close()
	return c.resolveFile(file)
}

func Run(ctx context.Context) error {
	globalLock.Lock()
	if started {
		return fmt.Errorf("config instance is already started")
	}
	started = true
	defer globalLock.Unlock()
	glog.Info("starting config instance")
	timer := time.NewTicker(scanInterval)
	instance.subscribers = make(map[string]chan struct{})
	instance.conf = new(Config)
	go func() {
		defer timer.Stop()
		for {
			select {
			case <-timer.C:
				if err := instance.scan(); err != nil {
					glog.Error("config scanning err: ", err)
				}
			case <-ctx.Done():
				return
			}
		}
	} ()
	return nil
}

func Conf() *Config {
	// FIXME: guard maybe useless @see Release()
	instance.guard.RLock()
	defer instance.guard.RUnlock()
	return instance.conf
}

// func Release() {

// }

// In this subscribe will send one event in the end
func Subscribe(user string, channel chan struct{}) error {
	instance.guard.Lock()
	defer instance.guard.Unlock()
	if _, ok := instance.subscribers[user]; ok {
		return fmt.Errorf("already subscribed")
	}
	glog.V(8).Info("user:", user, " subscribe config instance")
	instance.subscribers[user] = channel
	channel <- struct{}{}
	glog.V(9).Info("send to ", user, " update event after subscribe")
	return nil
}

func (c *config) resolveFile(file *os.File) error {
	glog.V(10).Info("resolve file")
	content, err := ioutil.ReadAll(file)
	if err != nil {
		return fmt.Errorf("resolve config err: %v", err)
	}

	calc := md5.New()
	sum := calc.Sum(content)
	if bytes.Equal(sum, c.sum) {
		return nil
	}
	glog.V(8).Info("detect config changed")

	var conf Config
	if err := yaml.Unmarshal(content, &conf); err != nil {
		glog.Error("resolve yaml file failed")
		return err
	}

	c.guard.Lock()
	c.conf = &conf
	c.sum = sum
	c.guard.Unlock()

	glog.V(8).Info("notify config changed")
	c.notifyAll()
	return nil
}

func (c *config) notifyAll() {
	for user, sub := range c.subscribers {
		glog.V(8).Info("notify user:", user, " config changed")
		sub <- struct{}{}
	}
}

// func (c *config) GeneratedCRD(types []string) error {

// 	return nil
// }

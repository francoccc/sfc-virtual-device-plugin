/*
 * @Descripition: generate kubeconfig
 * @Author: Franco Chen
 * @Date: 2022-09-23 14:32:39
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-09-23 14:58:22
 */
package main

import (
	"encoding/base64"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"strings"
)

const userRWPermission = 0600

const (
	solarflareConfigVal   = "solarflare-dir"
	k8sCAFilePathVarName  = "kube-ca-file"
	k8sServiceHostVarName = "k8s-service-host"
	k8sServicePortVarName = "k8s-service-port"
	serviceAccountPath    = "/var/run/secrets/kubernetes.io/serviceaccount"
	skipTLSVerifyVarName  = "skip-tls-verify"
)

const (
	defaultSolarflareConfigDir   = "/etc/solarflare"
	defaultK8sCAFilePath  = ""
	defaultK8sServiceHost = ""
	defaultK8sServicePort = 0
	defaultSkipTLSValue   = false
)

var (
	k8sServiceHost = flag.String(k8sServiceHostVarName, defaultK8sServiceHost, "Cluster IP of the kubernetes service")
	k8sServicePort = flag.Int(k8sServicePortVarName, defaultK8sServicePort, "Port of the kubernetes service")
	skipTLSVerify = flag.Bool(skipTLSVerifyVarName, defaultSkipTLSValue, "Should TLS verification be skipped")
	kubeCAFilePath = flag.String(k8sCAFilePathVarName, defaultK8sCAFilePath, "Override the default kubernetes CA file path")
	solarflareConfigDir = flag.String(solarflareConfigVal, defaultSolarflareConfigDir, "SolarFlare config dir")
)

func main() {
	flag.Parse()

	if *k8sServiceHost == defaultK8sServiceHost {
		logInvalidArg("must provide the k8s service cluster port")
	}
	if *k8sServicePort == defaultK8sServicePort {
		logInvalidArg("must provide the k8s service cluster port")
	}
	if *kubeCAFilePath == defaultK8sServiceHost {
		*kubeCAFilePath = serviceAccountPath + "/ca.crt"
	}

	tlsCfg := "insecure-skip-tls-verify: true"
	if !*skipTLSVerify {
		kubeCAFileContents, err := k8sCAFileContentsBase64(*kubeCAFilePath)
		if err != nil {
			logError("failed grabbing CA file: %w", err)
		}
		tlsCfg = "certificate-authority-data: " + kubeCAFileContents
	}

	if err := prepareSolarflareConfigDir(*solarflareConfigDir); err != nil {
		logError("failed to create SolareFlare config dir: %w", err)
	}

	kubeConfigFilePath := *solarflareConfigDir + "/vsfc-cdp.kubeconfig"
	serviceAccountToken, err := k8sKubeConfigToken(serviceAccountPath + "/token")
	if err != nil {
		logError("failed grabbing k8s token: %w", err)
	}

	if err := writeKubeConfig(kubeConfigFilePath, "https", *k8sServiceHost, *k8sServicePort, tlsCfg, serviceAccountToken); err != nil {
		logError("failed generating kubeconfig: %w", err)
	}
}

func writeKubeConfig(outputPath string, protocol string, k8sServiceIP string, k8sServicePort int, tlsConfig string, serviceAccountToken string) error {
	kubeConfigTemplate := `
# Kubeconfig file for Solarflare-Macvlan CNI plugin.
apiVersion: v1
kind: Config
clusters:
- name: local
  cluster:
    server: %s://[%s]:%d
    %s
users:
- name: vsfc-cdp
  user:
    token: "%s"
contexts:
- name: vsfc-cdp-context
  context:
    cluster: local
    user: vsfc-cdp
current-context: vsfc-cdp-context
`
	kubeconfig := fmt.Sprintf(kubeConfigTemplate, protocol, k8sServiceIP, k8sServicePort, tlsConfig, serviceAccountToken)
	logInfo("Generated KubeConfig saved to %s: \n%s", outputPath, kubeconfig)
	return ioutil.WriteFile(outputPath, []byte(kubeconfig), userRWPermission)
}

func k8sCAFileContentsBase64(pathCAFile string) (string, error) {
	data, err := ioutil.ReadFile(pathCAFile)
	if err != nil {
		return "", fmt.Errorf("failed reading file %s: %w", pathCAFile, err)
	}
	return strings.Trim(base64.StdEncoding.EncodeToString(data), "\n"), nil
}

func prepareSolarflareConfigDir(solarflareConfigDirPath string) error {
	return os.MkdirAll(solarflareConfigDirPath, userRWPermission)
}

func k8sKubeConfigToken(tokenPath string) (string, error) {
	data, err := ioutil.ReadFile(tokenPath)
	if err != nil {
		return "", fmt.Errorf("failed reading file %s: %w", tokenPath, err)
	}
	return string(data), nil
}

func logInvalidArg(format string, values ...interface{}) {
	log.Printf("ERROR: %s", fmt.Errorf(format, values...).Error())
	flag.PrintDefaults()
	os.Exit(1)
}

func logError(format string, values ...interface{}) {
	log.Printf("ERROR: %s", fmt.Errorf(format, values...).Error())
	os.Exit(1)
}

func logInfo(format string, values ...interface{}) {
	log.Printf("INFO: %s", fmt.Sprintf(format, values...))
}

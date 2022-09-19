/*
 * @Descripition: Plugin registry
 * @Author: Franco Chen
 * @Date: 2022-09-16 16:18:40
 * @LastEditors: Franco Chen
 * @LastEditTime: 2022-09-19 16:52:39
 */
package deviceplugin


var (
	ResourceRegistry = map[string]func(string, string) PluginImpl{
		"highfortfunds.com/vsfc": NewPlugin,
		"highfortfunds.com/vsfc-mem": NewMemVSfcPlugin,
		"highfortfunds.com/vsfc-handle": NewPlugin,
	}
)

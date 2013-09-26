@%@UCRWARNING=// @%@

@!@
if configRegistry.get("ucc/proxy/autoconfig/url", ""):
	print 'lockPref("network.proxy.autoconfig_url", "%s");' % configRegistry["ucc/proxy/autoconfig/url"]
	print 'lockPref("network.proxy.type", 2);'
	print 'lockPref("network.proxy.share_proxy_settings", true);'
	print 'lockPref("network.proxy.no_proxies_on", "localhost, 127.0.0.1");'
elif configRegistry.get("ucc/proxy/http", ""):
	list = configRegistry["ucc/proxy/http"].split(":")
	server = ":".join(list[0:-1])
	port = list[-1]
	if server and port:
		server = server.replace("http://", "")
		print 'lockPref("network.proxy.type", 1);'
		print 'lockPref("network.proxy.http", "%s");' % server
		print 'lockPref("network.proxy.http_port", %s);' % port
		print 'lockPref("network.proxy.ftp", "%s");' % server
		print 'lockPref("network.proxy.ftp_port", %s);' % port
		print 'lockPref("network.proxy.socks", "%s");' % server
		print 'lockPref("network.proxy.socks_port", %s);' % port
		print 'lockPref("network.proxy.ssl", "%s");' % server
		print 'lockPref("network.proxy.ssl_port", %s);' % port
		print 'lockPref("network.proxy.share_proxy_settings", true);'
		print 'lockPref("network.proxy.no_proxies_on", "localhost, 127.0.0.1");'
@!@

@%@UCRWARNING=// @%@

@!@
if configRegistry.get("ucc/proxy/autoconfig/url", ""):
	print 'lockPref("network.proxy.autoconfig_url", "%s");' % configRegistry["ucc/proxy/autoconfig/url"]
	print 'lockPref("network.proxy.type", 2);'
elif configRegistry.get("ucc/proxy/http", ""):
	list = configRegistry["ucc/proxy/http"].split(":")
	server = ":".join(list[0:-1])
	port = list[-1]
	if server and port:
		server = server.replace("http://", "")
		print 'user_pref("network.proxy.type", 1);'
		print 'user_pref("network.proxy.http", "%s");' % server
		print 'user_pref("network.proxy.http_port", %s);' % port
		print 'user_pref("network.proxy.ftp", "%s");' % server
		print 'user_pref("network.proxy.ftp_port", %s);' % port
		print 'user_pref("network.proxy.socks", "%s");' % server
		print 'user_pref("network.proxy.socks_port", %s);' % port
		print 'user_pref("network.proxy.ssl", "%s");' % server
		print 'user_pref("network.proxy.ssl_port", %s);' % port

@!@

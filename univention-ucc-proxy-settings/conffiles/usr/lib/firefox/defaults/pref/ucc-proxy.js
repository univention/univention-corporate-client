@%@UCRWARNING=// @%@

@!@
if configRegistry.get("proxy/pac", ""):
	print 'lockPref("network.proxy.autoconfig_url", "%s");' % configRegistry["proxy/pac"]
	print 'lockPref("network.proxy.type", 2);'
elif configRegistry.get("proxy/http", ""):
	list = configRegistry["proxy/http"].split(":")
	server = ":".join(list[0:-1])
	port = list[-1]
	server = server.replace("http://", "")
	print 'user_pref("network.proxy.http", "%s");' % server
	print 'user_pref("network.proxy.http_port", %s);' % port
	print 'user_pref("network.proxy.type", 1);'
@!@

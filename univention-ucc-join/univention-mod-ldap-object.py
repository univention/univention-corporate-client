#!/usr/bin/python2.7
# -*- coding: utf-8 -*-
#
# Copyright 2012-2014 Univention GmbH
#
# http://www.univention.de/
#
# All rights reserved.
#
# The source code of this program is made available
# under the terms of the GNU Affero General Public License version 3
# (GNU AGPL V3) as published by the Free Software Foundation.
#
# Binary versions of this program provided by Univention to you as
# well as other copyrighted, protected or trademarked materials like
# Logos, graphics, fonts, specific documentations and configurations,
# cryptographic keys etc. are subject to a license agreement between
# you and Univention and not subject to the GNU AGPL V3.
#
# In the case you use this program under the terms of the GNU AGPL V3,
# the program is provided in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public
# License with the Debian GNU/Linux or Univention distribution in file
# /usr/share/common-licenses/AGPL-3; if not, see
# <http://www.gnu.org/licenses/>.

import ldap
import sys
import os
import optparse
import ldap.modlist
import univention.config_registry

ucr = univention.config_registry.ConfigRegistry()
ucr.load()

TLS_CACERTFILE = "/etc/univention/ssl/ucsCA/CAcert.pem"

usage = "usage:\n"
usage += "  %prog [options]\n\n"
usage += "description:\n"
usage += "  Adds value to ldap attribute of a ldap object."

def failed(msg):

	sys.stderr.write("%s\n" % msg)
	sys.exit(1)


def modifyObject(binddn, bindpw, dn, attr, value, replace):

	server = ucr.get("ldap/master")
	port = ucr.get("ldap/master/port", "7389")
	base = ucr.get("ldap/base")

	if server and port and base:

		uri = "ldap://%s:%s" % (server, port)

		try:
			lo = ldap.initialize(uri)
			lo.set_option( ldap.OPT_X_TLS_CACERTFILE, TLS_CACERTFILE)
			lo.start_tls_s()
		except Exception, e:
			lo.unbind_s()
			failed("failed to connect to ldap uri %s (%s)" % (uri, e))


		try:
			lo.simple_bind_s(binddn, bindpw)
		except Exception, e:
			lo.unbind_s()
			failed("failed to bind as %s (%s)" % (binddn, e))

		try:
			result = lo.search_s(dn, ldap.SCOPE_BASE, '(objectClass=*)', [])
		except Exception, e:
			lo.unbind_s()
			failed("failed to find %s (%s)" % (dn, e))

		old = []
		if result and len(result) > 0:
			old = result[0][1].get(attr, [])

		if not value in old or replace:
			if replace:
				new = [value]
			else:
				new = old + [value]
			ldif = ldap.modlist.modifyModlist({attr:old}, {attr:new})

			try:
				lo.modify_s(dn, ldif)
			except Exception, e:
				lo.unbind_s()
				failed("failed to modify %s with %s (%s)" % (dn, ldif, e))

		lo.unbind_s()


if __name__ == '__main__':

	parser = optparse.OptionParser(usage=usage)
	parser.add_option("-D", "--binddn", dest="binddn", help="bind dn", action="store", type="string")
	parser.add_option("-w", "--bindpwd", dest="bindpw", help="bind password", action="store", type="string")
	parser.add_option("-d", "--dn", dest="dn", help="dn to modify", action="store", type="string")
	parser.add_option("-a", "--attribute", dest="attr", help="attribute to modify", action="store", type="string")
	parser.add_option("-v", "--value", dest="value", help="value to append to attribute", action="store", type="string")
	parser.add_option("-r", "--replace", dest="replace", help="replace attribute with the new value", default=False, action="store_true")

	options, args = parser.parse_args()

	if not options.binddn or not options.bindpw or not options.dn or not options.attr or not options.value:
		parser.print_help()
		sys.exit(1)

	modifyObject(
		options.binddn,
		options.bindpw,
		options.dn,
		options.attr,
		options.value,
		options.replace)

	sys.exit(0)

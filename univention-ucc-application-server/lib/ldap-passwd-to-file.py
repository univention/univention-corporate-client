#!/usr/bin/python2.7
# -*- coding: utf-8 -*-
#
# Univention PAM
#   Dump all ldap groups with members to a single file
#
# Copyright 2011-2013 Univention GmbH
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

import univention.uldap
import optparse
import shutil
import sys
import os
import tempfile

if __name__ == '__main__':
	parser = optparse.OptionParser( )
	parser.add_option("--file", dest="file", default='/var/lib/extrausers/passwd',
		action="store", help="write result to the given file, default is /var/lib/extrausers/passwd")
	parser.add_option("--verbose", dest="verbose",
		default=False, action="store_true", help="verbose output")
	(options, args) = parser.parse_args()

	try:
		lo = univention.uldap.getMachineConnection( ldap_master=False )
	except ldap.SERVER_DOWN:
		print "Abort: Can't contact LDAP server."
		sys.exit(1)

	dirName = os.path.dirname(options.file)
	if not os.path.isdir(dirName):
		os.makedirs(dirName)

	# passwd:
	# uid:x:uidNumber:gidNumber:gecos:homeDirectory:loginShell

	result = []
	attr = ['uid', 'uidNumber', 'gidNumber', 'gecos', 'homeDirectory', 'loginShell']
	users = lo.search('objectClass=posixAccount', attr=attr)
	if options.verbose:
		print 'Found %d ldap users' % len(users)

	if len(users) < 1:
		print 'Abort: Did not found any LDAP user.'
		sys.exit(1)
	

	# Write to a temporary file
	(fdtemp, fdname) = tempfile.mkstemp()
	fd = os.fdopen(fdtemp, 'w')

	for user in users:
		uid = user[1].get('uid', [''])[0]
		uidNumber = user[1].get('uidNumber', [''])[0]
		gidNumber = user[1].get('gidNumber', [''])[0]
		gecos = user[1].get('gecos', [''])[0]
		homeDirectory = user[1].get('homeDirectory', [''])[0]
		loginShell = user[1].get('loginShell', [''])[0]
		passwd = '%s:x:%s:%s:%s:%s:%s' % (uid, uidNumber, gidNumber, gecos, homeDirectory, loginShell)
		fd.write('%s\n' % passwd)
	fd.close()

	os.chmod(fdname, 0644)

	# Move the file
	shutil.move(fdname, options.file)
	if options.verbose:
		print 'The file %s was created.' % options.file

	sys.exit(0)


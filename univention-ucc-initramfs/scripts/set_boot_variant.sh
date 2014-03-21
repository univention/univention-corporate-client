#!/bin/bash -e
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

usage ()
{
	echo "$0 <boot variant>"
	echo "Set boot variant for the local UCC client to given variant and remove the repartition flag."
	echo "Possible values are:"
	echo "		- none"
	echo "		- overlayfs"
	echo "		- rollout"
}

if [ -z "$1" ]; then
	usage
	exit 1
fi


eval "$(ucr shell)"

boot_variant="$1"

if [ -z "$ldap_hostdn" ] || [ ! -e /etc/machine.secret ]; then
	echo "Error: The client is not joinined."
	exit 1
fi

case "$boot_variant" in
	none | overlayfs | rollout )
		/usr/share/univention-join/univention-mod-ldap-object.py --binddn="$ldap_hostdn" --bindpwd="$(</etc/machine.secret)" \
    			--dn "$ldap_hostdn" \
    			--attribute "univentionCorporateClientBootVariant" \
    			--value "$boot_variant" \
				--replace

		/usr/share/univention-join/univention-mod-ldap-object.py --binddn="$ldap_hostdn" --bindpwd="$(</etc/machine.secret)" \
    			--dn "$ldap_hostdn" \
    			--attribute "univentionCorporateClientBootRepartitioning" \
    			--value "FALSE" \
				--replace
		;;
	*)
		echo "Unknown parameter: $boot_variant"
		usage 
		exit 1
		;;
esac

exit 0

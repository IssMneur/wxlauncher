/*
Copyright (C) 2009-2010 wxLauncher Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef PROFILEMANAGEROPERATOR_H
#define PROFILEMANAGEROPERATOR_H

/** ProManOperator: Mechanism to manipulate profiles through cmd line options.
 none indicates that this feature is not in use and that normal operation
 should occur instead. */

namespace ProManOperator
{

enum profileOperator {
	none = 0,
	add,
	select,
	invalid
};

int RunProfileOperator(profileOperator op);

};

#endif
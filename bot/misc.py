#*************************************************************************
#*            Atrinik, a Multiplayer Online Role Playing Game            *
#*                                                                       *
#*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
#*                                                                       *
#* Fork from Crossfire (Multiplayer game for X-windows).                 *
#*                                                                       *
#* This program is free software; you can redistribute it and/or modify  *
#* it under the terms of the GNU General Public License as published by  *
#* the Free Software Foundation; either version 2 of the License, or     *
#* (at your option) any later version.                                   *
#*                                                                       *
#* This program is distributed in the hope that it will be useful,       *
#* but WITHOUT ANY WARRANTY; without even the implied warranty of        *
#* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
#* GNU General Public License for more details.                          *
#*                                                                       *
#* You should have received a copy of the GNU General Public License     *
#* along with this program; if not, write to the Free Software           *
#* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
#*                                                                       *
#* The author can be reached at admin@atrinik.org                        *
#*************************************************************************

## @file
## Miscellaneous functions.

import subprocess

## NDI flags used for the drawinfo2 command.
class NDI:
	WHITE = "ffffff"
	DK_ORANGE = "ff6600"

	SAY = 0x0100
	SHOUT = 0x0200
	TELL = 0x0400
	PLAYER = 0x0800
	EMOTE = 0x01000
	ANIM = 0x02000

## Execute a command.
## @param cmd List of command arguments (and the command itself).
def command_execute(cmd):
	# Try to execute the command.
	try:
		try:
			return subprocess.check_output(cmd, stderr = subprocess.STDOUT).decode()
		except AttributeError:
			return subprocess.Popen(cmd, stdout = subprocess.PIPE, stderr = subprocess.STDOUT).communicate()[0].decode()
	# Command didn't return 0.
	except subprocess.CalledProcessError:
		pass
	# Command not found.
	except OSError:
		pass

	return None

# CFBoard.py - CFBoard class
#
# Copyright (C) 2002,2003 Joris Bontje, Michael Toennies
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# The author can be reached via e-mail at jbontje@suespammers.org
#

import shelve

class CFBoard:

	boarddb_file = './data/crossfireboard'
	boarddb = {}
	total = 0

	def __init__(self):
		self.boarddb = shelve.open(self.boarddb_file)

	def write(self, boardname, author, message):
		if not self.boarddb.has_key(boardname):
			self.boarddb[boardname]=[[author,message]]
		else:
			temp=self.boarddb[boardname]
			temp.append([author,message])
			self.boarddb[boardname]=temp

	def close(self):
		self.boarddb.close()

	def list(self, boardname):
		if self.boarddb.has_key(boardname):
			elements=self.boarddb[boardname]
			return elements
			
	
	def delete(self, boardname, id):
		if self.boarddb.has_key(boardname):
			if id>0 and id<=len(self.boarddb[boardname]):
				temp=self.boarddb[boardname]
				temp.pop(id-1)
				self.boarddb[boardname]=temp
				return 1
		return 0

        def countmsg(self, boardname):
                if self.boarddb.has_key(boardname):
                        num = len(self.boarddb[boardname])
                        return num
                else:
                        return 0
	
	def getauthor(self, boardname, id):
		if self.boarddb.has_key(boardname):
			if id>0 and id<=len(self.boarddb[boardname]):
				author,message=self.boarddb[boardname][id-1]
				return author

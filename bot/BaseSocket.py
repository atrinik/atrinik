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
## Base socket class other socket-using classes inherit from.

import time, socket, threading

## The base socket class; implements generic socket code.
class BaseSocket:
	## Initializes the base socket.
	def __init__(self):
		ts = time.time()
		self._reconnect = 0
		self._command_queue = []
		self._command_queue_stamp = ts
		self._command_queue_lock = threading.RLock()
		self._connect()

	## Connect to a server.
	def _connect(self):
		# Create the socket.
		self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

		# Try to connect.
		try:
			self.socket.connect((self.host, self.port))
		# Failed, try to reconnect later.
		except socket.error:
			self.disconnect()
			print("{0}@{1}:{2} could not connect, marking for reconnect.".format(self.name, self.host, self.port))
			self._reconnect = time.time() + self.config.getfloat(self.section, "reconnect_timeout")
			return False

		# Do custom post-connect code.
		self._post_connect()

		return True

	## Post-connect function, called after a connection has been established
	## successfully in ::_connect.
	def _post_connect(self):
		pass

	## Disconnects the socket.
	def disconnect(self):
		self.socket.close()

	## Closes the class. Should be called eventually.
	def close(self):
		self.disconnect()

	## Mark this socket for reconnection.
	def _mark_reconnect(self):
		self.disconnect()
		print("{0}@{1}:{2} has been disconnected from server, marking for reconnect.".format(self.name, self.host, self.port))
		self._reconnect = time.time() + self.config.getfloat(self.section, "reconnect_timeout")

	## Handles reconnection, if possible.
	## @return True if we are (re-)connected to a server and can do data
	## receiving/sending/handling/etc, False otherwise.
	def handle_reconnect(self):
		if self._reconnect:
			if time.time() >= self._reconnect:
				self._reconnect = 0

				if not self._connect():
					return False
			else:
				return False

		return True

	## Handle the command queue. The queue is a list that has items added to
	## the end, and when the time comes (in this function), the first element
	## is removed from the list and sent to the server.
	def _command_queue_handle(self):
		ts = time.time()

		# Acquire lock.
		self._command_queue_lock.acquire()

		# Anything in the queue, and has enough time passed yet?
		if self._command_queue and ts >= self._command_queue_stamp:
			# Remove the first element.
			(cmd, delay) = self._command_queue.pop(0)
			# Handle the command.
			self._command_queue_handler(cmd)
			# Update the time that must pass until we can send another
			# command in this queue.
			self._command_queue_stamp = ts + delay

		self._command_queue_lock.release()

	## Queue command handler. By default, the command is sent to the server
	## as-is.
	## @param cmd The command.
	def _command_queue_handler(self, cmd):
		if type(cmd) != type(bytes()):
			cmd = cmd.encode()

		self.socket.send(cmd)

	## Add a command to the command queue.
	## @param cmd The command to add.
	## @param delay Integer or float delay that must pass before the next
	## added command may be executed.
	def command_queue_add(self, cmd, delay):
		self._command_queue_lock.acquire()
		self._command_queue.append((cmd, delay))
		self._command_queue_lock.release()

	## Handle socket data.
	def handle_data(self):
		pass

	## Extra handling after data handling is done.
	def handle_extra(self):
		self._command_queue_handle()

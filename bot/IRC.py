## @file
## Handle IRC related functions.

from BaseSocket import *

## The IRC class.
class IRC(BaseSocket):
	## Initialize the IRC class.
	## @param data Tuple containing host, port, nickname, password and channels to join.
	## @param config ConfigParser instance.
	## @param section Section in the config with the settings for this bot.
	def __init__(self, data, config, section):
		(self.host, self.port, self.name, self.pswd, self.channels) = data
		self.config = config
		self.section = section
		BaseSocket.__init__(self)
		# Set non-blocking socket mode.
		self.socket.setblocking(0)

		# Identify the bot.
		self.socket.send("USER {0} {0} {0} {0} :{0}\n".format(self.name).encode())
		self.socket.send("NICK {0}\n".format(self.name).encode())
		self.socket.send("PRIVMSG NickServ : IDENTIFY {0}\n".format(self.pswd).encode())

		# Join specified channels.
		for channel in self.channels:
			self.socket.send("JOIN {0}\n".format(channel).encode())

	## Handle IRC data.
	def handle_data(self):
		# Try to get data.
		try:
			data = self.socket.recv(2048)
		except socket.error:
			return

		# No data means we lost connection.
		if not data:
			self._mark_reconnect()
			return

		try:
			ex = data.decode().split(" ")
		except UnicodeDecodeError:
			return

		# Play ping<->pong with the server.
		if ex[0] == "PING":
			self.socket.send("PONG {0}\n".format(ex[1]))

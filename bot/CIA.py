## @file
## Handles CIA.vc bot interaction.

import xmlrpclib, socket

class CIA:
	def __init__(self):
		pass

	def submit(self, xml):
		server = xmlrpclib.ServerProxy("http://cia.navi.cx")

		try:
			server.hub.deliver(xml)
		except:
			pass

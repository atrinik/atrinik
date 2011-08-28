class Interface:
	_msg = ""
	_links = []
	_text_input = None

	def __init__(self, activator, npc):
		self._activator = activator
		self._npc = npc
		self._icon = npc.arch.clone.face[0]
		self._title = npc.name

	def add_msg(self, msg):
		self._msg += msg

	def add_link(self, link):
		self._links.append("<a>" + link + "</a>")

	def add_link2(self, link):
		self._links.append(link)

	def set_icon(self, icon):
		self._icon = icon

	def set_title(self, title):
		self._title = title

	def set_text_input(self, text_input = ""):
		self._text_input = text_input

	def finish(self):
		if not self._msg:
			return

		pl = self._activator.Controller()

		if pl.s_socket_version >= 1058:
			pl.SendInterface(self._msg, self._links, self._icon, self._title, self._text_input)
		else:
			self._me.SayTo(self._activator, "not implemented")

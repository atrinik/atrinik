class Interface:
	def __init__(self, activator, npc):
		self._msg = ""
		self._links = []
		self._text_input = None
		self._activator = activator
		self._npc = npc
		self._icon = npc.arch.clone.face[0]
		self._title = npc.name

	def add_msg(self, msg, color = None, newline = True):
		if newline and self._msg:
			self._msg += "\n\n"

		if color:
			self._msg += "<c=#" + color + ">"

		self._msg += msg

		if color:
			self._msg += "</c>"

	def add_msg_icon(self, icon, desc = ""):
		self._msg += "\n\n"
		self._msg += "<bar=#000000 54 54><border=#606060 54 54><x=2><y=2><icon="
		self._msg += icon
		self._msg += " 50 50><x=-2><y=-2>"
		self._msg += "<padding=60><hcenter=50>"
		self._msg += desc
		self._msg += "</hcenter></padding>"

	def add_msg_icon_object(self, obj):
		self.add_msg_icon(obj.face[0], obj.GetName())

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

	def add_objects(self, objs):
		if type(objs) != list:
			objs = [objs]

		for obj in objs:
			obj = obj.Clone()
			self.add_msg_icon_object(obj)
			obj.InsertInto(self._activator)

	def finish(self):
		if not self._msg:
			return

		pl = self._activator.Controller()
		SetReturnValue(1)

		if pl.s_socket_version >= 1058:
			pl.SendInterface(self._msg, self._links, self._icon, self._title, self._text_input)
		else:
			self._npc.SayTo(self._activator, "\n" + self._msg)

			if self._links:
				self._npc.SayTo(self._activator, "\n" + "\n".join(self._links), True)

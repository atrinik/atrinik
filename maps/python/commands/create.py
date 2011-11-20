## @file
## Implements the /create DM command.

import re

def main():
	msg = WhatIsMessage()

	if not msg:
		return

	match = re.match(r"(?:(\d+) )?(\w+)(?: of (\w+))?(?: (.+))?", msg)

	if not match:
		return

	(num, archname, artname, attribs) = match.groups()

	obj = CreateObject(archname)
	obj.f_identified = True

	if not obj:
		return

	if num:
		obj.nrof = int(num)

	if artname:
		obj.Artificate(artname)

	if attribs:
		for (attrib, val) in re.findall(r'(\w+) ("[^"]+"|[^ ]+)', attribs):
			if val.startswith('"') and val.endswith('"'):
				val = val[1:-1]

			try:
				val = int(val)
			except ValueError:
				try:
					val = float(val)
				except ValueError:
					pass

			if val == "None":
				val = None

			if hasattr(obj, attrib):
				setattr(obj, attrib, val)
			elif hasattr(obj, "f_" + attrib):
				setattr(obj, "f_" + attrib, True if val else False)
			else:
				obj.WriteKey(attrib, str(val))

	if obj.f_monster:
		activator.map.Insert(obj, activator.x, activator.y)
		obj.Fix()
	else:
		obj.InsertInto(activator)

if activator.f_wiz or "create" in activator.Controller().cmd_permissions:
	activator.speed_left += 1.0
	main()

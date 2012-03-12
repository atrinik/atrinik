## @file
## Implements the /create DM command.

import re

def main():
	msg = WhatIsMessage()

	if not msg:
		return

	match = re.match(r"(?:(\d+) )?([^ ]+)(?: of (\w+))?(?: (.+))?", msg)

	if not match:
		return

	(num, archname, artname, attribs) = match.groups()

	if not num:
		num = 1
	else:
		num = int(num)

	for i in range(num):
		try:
			obj = CreateObject(archname)
		except AtrinikError as err:
			pl.DrawInfo(str(err), COLOR_RED)
			break

		obj.f_identified = True

		if artname:
			try:
				obj.Artificate(artname)
			except AtrinikError as err:
				obj.Destroy()
				pl.DrawInfo(str(err), COLOR_RED)
				break

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

			if obj.randomitems:
				obj.CreateTreasure(obj.randomitems, obj.level)
		else:
			obj.InsertInto(activator)

main()

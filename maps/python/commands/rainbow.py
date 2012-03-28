## @file
## The /rainbow command.

import colorsys, random

def main():
	if not msg:
		return

	newmsg = ""

	for c in msg:
		(h, s, v) = (random.randint(0, 255) / 255, random.randint(0, 192) / 255, random.randint(168, 255) / 255)
		(r, g, b) = colorsys.hsv_to_rgb(h, s, v)
		newmsg += "<c=#%2X%2X%2X>%s</c>" % (255 * r, 255 * g, 255 * b, c)

	activator.map.DrawInfo(activator.x, activator.y, newmsg, COLOR_WHITE, CHAT_TYPE_LOCAL, activator.name)

main()

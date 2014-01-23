## @file
## Implements the /mapinfo command.

def main():
	for msg in [
		"{name} ({bg_music}, {path}, x: {x}, y: {y})".format(
			name = activator.map.name,
			bg_music = activator.map.bg_music,
			path = activator.map.path,
			x = activator.x,
			y = activator.y,
		),
		"Players: {num_players} difficulty: {difficulty} size: "
		"{width}x{height} start: {enter_x}x{enter_y}".format(
			num_players = activator.map.CountPlayers(),
			difficulty = activator.map.difficulty,
			width = activator.map.width,
			height = activator.map.height,
			enter_x = activator.map.enter_x,
			enter_y = activator.map.enter_y,
		),
		activator.map.msg,
	]:
		if msg:
			activator.Controller().DrawInfo(msg, color = COLOR_WHITE)

main()

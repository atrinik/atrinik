## @file
## Implements packet-specific functions, such as sending a notification
## command to the player's client or creating a book GUI, for example.
##
## @author Alex Tokar

## Send a notification command to the player.
## @param pl The player.
## @param msg Message contents of the notification.
## @param action Optional macro or command to execute when the
## notification is triggered.
## @param shortcut Macro shortcut to temporarily use as trigger for the
## notification's action, for example, "?HELP".
## @param delay How long the delay should stay visible, in milliseconds.
def Notification(pl, msg, action = None, shortcut = None, delay = 0):
	fmt = "Bs"
	data = [0, msg]

	# Add the action.
	if action:
		fmt += "Bs"
		data += [1, action]

		# Add the shortcut, but only if there is an action.
		if shortcut:
			fmt += "Bs"
			data += [2, shortcut]

	# Add the delay.
	if delay:
		fmt += "Bi"
		data += [3, delay]

	# Send it off...
	pl.SendPacket(27, fmt, *data)

## Send a map stats command.
## @pl Player to send the command to.
## @name Map name to update.
## @music Map music to update.
## @weather Map weather to update.
def MapStats(pl, name = None, music = None, weather = None):
	fmt = ""
	data = []

	# Add map name.
	if name:
		fmt += "Bs"
		data += [1, name]

	# Add map music.
	if music:
		fmt += "Bs"
		data += [2, music]

	# Add map weather.
	if weather:
		fmt += "Bs"
		data += [3, weather]

	# Send it off...
	pl.SendPacket(13, fmt, *data)

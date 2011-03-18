## @file
## Miscellaneous functions.

import subprocess

## NDI flags used for the drawinfo2 command.
class NDI:
	WHITE = 0
	DK_ORANGE = 17

	SAY = 0x0100
	SHOUT = 0x0200
	TELL = 0x0400
	PLAYER = 0x0800
	EMOTE = 0x01000
	ANIM = 0x02000

## Execute a command.
## @param cmd List of command arguments (and the command itself).
def command_execute(cmd):
	# Try to execute the command.
	try:
		try:
			return subprocess.check_output(cmd, stderr = subprocess.STDOUT).decode()
		except AttributeError:
			return subprocess.Popen(cmd, stdout = subprocess.PIPE, stderr = subprocess.STDOUT).communicate()[0].decode()
	# Command didn't return 0.
	except subprocess.CalledProcessError:
		pass
	# Command not found.
	except OSError:
		pass

	return None

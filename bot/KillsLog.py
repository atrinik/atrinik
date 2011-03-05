## @file
## Handles kills log.

## Kills logging.
class KillsLog:
	## Initialize the class.
	## @param bot The associated bot.
	def __init__(self, bot):
		self._bot = bot

		# Initialize the dictionary if it doesn't exist.
		if not "kills" in self._bot.db:
			self._bot.db["kills"] = {}

	## Initialize a new killer in the dictionary.
	##
	## If the killer already exists, it will not be re-initialized.
	## @param name Name of the killer.
	def _init_killer(self, name):
		# Check that it doesn't exist yet.
		if name.lower() in self._bot.db["kills"]:
			return

		# Initialize the dictionary for this killer. The killer's name is
		# lowercased when used as a key, for easier searching.
		self._bot.db["kills"][name.lower()] = {
			"name": name,
			"normal": [],
			"arena": [],
		}

	## Log a kill.
	## @param name Killer's name.
	## @param victim The victim that has died.
	## @param how How the victim died -- for example, "with firestorm".
	## @param pvp If not None, this is a PvP kill.
	def kill_log(self, name, victim, how, pvp):
		self._init_killer(name)
		name = name.lower()

		if pvp:
			where = self._bot.db["kills"][name]["arena"]
		else:
			where = self._bot.db["kills"][name]["normal"]

		where.append((victim, how))

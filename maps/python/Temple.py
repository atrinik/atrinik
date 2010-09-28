## @file
## Generic code for implementing temples.

from Atrinik import *

## A base temple object.
class BaseTemple:
	## Name of the god.
	name = "internal"
	## Description about the god; will be shown when player asks about
	## the god.
	desc = "xxx"
	## Name of the enemy god.
	enemy_name = None
	## Description about the enemy god that will be shown when asked about
	## the enemy god.
	enemy_desc = None
	## Extra message to display after the initial greeting, before services.
	hello_msg = None
	## Extra message to display after everything else, after services.
	hello_msg_after = None

	## The initializer.
	## @param activator Who activated the script.
	## @param me The event owner.
	def __init__(self, activator, me):
		self._activator = activator
		self._me = me

		# Initialize default services.
		#
		# List entry meanings:
		# - If True this is a spell and can be automatically handled,
		#   otherwise you need a special action in handle_temple().
		# - The amount of money required to pay for the service
		# - Row order. The highest will be as the first service, while
		# - the lowest will be the last service.
		self.services = {
			"remove depletion": [
				True,
				self._activator.level * 100,
				25,
			],
			"remove curse": [
				True,
				3000,
				20,
			],
			"remove damnation": [
				True,
				5000,
				15,
			],
			"cure disease": [
				True,
				self._activator.level * 40,
				10,
			],
			"cure poison": [
				True,
				self._activator.level * 5,
				5,
			],
			"food": [
				False,
				0,
				0,
			],
		}

		# Adjust prices for players under level 3.
		if self._activator.level < 3:
			self.services["remove depletion"][1] = self.services["cure disease"][1] = self.services["cure poison"][1] = 0

	## Post initializing. Allows changing the default services.
	def post_init(self):
		pass

	## Handle a spell service.
	## @param spell Spell name.
	def handle_spell(self, spell):
		if self._activator.PayAmount(self.services[spell][1]):
			self._me.SayTo(self._activator, "\nOk, I will cast {0} on you now.".format(spell))

			if self.services[spell][1]:
				self._activator.Write("You pay the money.", 0)

			self._me.CastAbility(self._activator, GetSpellNr(spell), CAST_NORMAL, 0, "")
		else:
			self._me.SayTo(self._activator, "\nYou do not have enough money.")

	## Check whether activator is of an enemy god.
	def is_enemy_god(self):
		# No enemy god, can't be an enemy.
		if not self.enemy_name:
			return False

		return self._activator.GetGod() == self.enemy_name

## Handle generic temple dialog.
## @param temple One of the TempleXxx classes.
## @param me Event owner.
## @param activator The event activator.
## @param msg What was said.
## @return True if we handled the chat, False otherwise.
def handle_temple(temple, me, activator, msg):
	# Initialize the temple.
	temple = temple(activator, me)
	# Post initialize.
	temple.post_init()

	if msg == "hi" or msg == "hey" or msg == "hello":
		me.SayTo(activator, "\nWelcome to the church of ^{0}^.".format(temple.name))

		# We don't like enemy gods.
		if temple.is_enemy_god():
			me.SayTo(activator, "\n... I see that you are a follower of ^{0}^. It would be best for you to leave immediately.".format(temple.enemy_name), True)
			return True

		# Custom message to say before services.
		if temple.hello_msg:
			me.SayTo(activator, temple.hello_msg, True)

		# Sort the services.
		services = sorted(temple.services, key = lambda key: temple.services[key][2], reverse = True)

		if services:
			me.SayTo(activator, "I can offer you the following services:\n", True)

			for service in services:
				me.SayTo(activator, "^{0}^ for {1}".format(service.capitalize(), temple.services[service][1] and CostString(temple.services[service][1]) or "free"), True)

		# Custom message to say after services.
		if temple.hello_msg_after:
			me.SayTo(activator, temple.hello_msg_after, True)

		return True

	# Explain about the temple's god.
	if msg == temple.name.lower():
		me.SayTo(activator, "\n" + temple.desc)
		return True

	# Explain about the temple's enemy god, if there is one.
	if temple.enemy_name and msg == temple.enemy_name.lower():
		me.SayTo(activator, "\n" + temple.enemy_desc)
		return True

	# No services for followers of enemy god.
	if temple.is_enemy_god():
		return False

	# Try to find the service.
	service = list(filter(lambda key: key == msg, temple.services))

	# Found it?
	if service:
		service = service[0]

		# Is it a non-spell service?
		if not temple.services[service][0]:
			# Give out free food.
			if service == "food":
				if activator.food < 500:
					activator.food = 500
					me.SayTo(activator, "\nYour stomach is filled again.")
				else:
					me.SayTo(activator, "\nYou don't look very hungry to me...")
		# Handle spell services.
		else:
			temple.handle_spell(service)

		return True

	return False

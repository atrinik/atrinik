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
			if self.services[spell][1]:
				self._activator.Write("You pay {}.".format(CostString(self.services[spell][1])), 0)

			self._me.SayTo(self._activator, "\nOk, I will cast {0} on you now.".format(spell))
			self._me.Cast(GetSpellNr(spell), self._activator)
		else:
			self._me.SayTo(self._activator, "\nYou do not have enough money.")

	## Check whether activator is of an enemy god.
	def is_enemy_god(self):
		# No enemy god, can't be an enemy.
		if not self.enemy_name:
			return False

		return self._activator.GetGod() == self.enemy_name

## Grunhilde.
class TempleGrunhilde(BaseTemple):
	name = "Grunhilde"
	desc = "I am a servant of the Valkyrie Queen and the Goddess of Victory, Grunhilde.\nIf you would like to join our Temple and fight for our cause, please touch the altar and Grunhilde will smile upon you."

## Dalosha.
class TempleDalosha(BaseTemple):
	name = "Dalosha"
	desc = "I am a servant of the first Queen of the Drow and Spider Goddess, Dalosha.\nIf you would like to join our Temple, please touch the altar and Dalosha will smile upon you."
	enemy_name = "Tylowyn"
	enemy_desc = "The high elves and their oppressive queen!  Do not be swayed by her traps, she started the war with her attempt to enforce proper elven conduct in war.  Tylowyn was too cowardly and weak to realize that it was our destiny to rule the world, so now she and her elves shall also perish!"

## Drolaxi.
class TempleDrolaxi(BaseTemple):
	name = "Drolaxi"
	desc = "I am a servant of Queen of the Chaotic Seas and the Goddess of Water, Drolaxi.\nIf you would like to join our Temple, please touch the altar and Drolaxi will smile upon you."
	enemy_name = "Shaligar"
	enemy_desc = "Flames and terror does he seek to spread.  Do not be deceived, although the flame be kin to the Lady, he is complerely mad.  Avoid the scorching flames or they will consume you.  We shall rule the world and all shall be seas!"

## Grumthar.
class TempleGrumthar(BaseTemple):
	name = "Grumthar"
	desc = "I am a servant of the First Dwarven Lord and the God of Smithery, Grumthar.\nIf you would like to join our Temple, please touch the altar and Grumthar will smile upon you."
	enemy_name = "Jotarl"
	enemy_desc = "Do not be speaking of that Giant tyrant amongst us.  Him and his giants have long sought to crush the little folk.  He has those goblin vermin under his wing also."

## Jotarl.
class TempleJotarl(BaseTemple):
	name = "Jotarl"
	desc = "I am a servant of the Titan King and the God of the Giants, Jotarl.\nIf you would like to join our Temple, please touch the altar and Jotarl will smile upon you."
	enemy_name = "Grumthar"
	enemy_desc = "Puny dwarves do not scare Jotarl with their technology and mithril weapons, we shall rule the caves!  The Dwarves shall fall and we shall claim their gold for ourselves."

## Moroch.
class TempleMoroch(BaseTemple):
	name = "Moroch"
	desc = "I am a servant of the Lord of the Grave and King of Undeath, Moroch.\nIf you would like to join our Temple, please touch the altar and Moroch will smile upon you."
	enemy_name = "Terria"
	enemy_desc = "Do you honestly believe the lies of those naturists?  The powers of undeath will rule the universe and the servants of Nature will fail.  The Dark Lord shall not fail to dominate the land and all be consumed in glorious Death."

## Rashindel.
class TempleRashindel(BaseTemple):
	name = "Rashindel"
	desc = "I am a servant of the Demonic King and the Overlord of Hell, Rashindel.\nIf you would like to join our Circle, please touch the altar and Rashindel will smile upon you."
	enemy_name = "Tabernacle"
	enemy_desc = "Accursed fool, do not mention that name in our presence!  In the days before this world, the Tyrant sought to oppress us with the his oppressive ideals of truth and justice.  After our master freed us from the simpleton lots who follow him, he was bound into the darkness which is now our glorious kingdom."

## Rogroth.
class TempleRogroth(BaseTemple):
	name = "Rogroth"
	desc = "I am a servant of the King of the Stormy Skies and the God of Lightning, Rogroth.\nIf you would like to join our Temple, please touch the altar and Rogroth will smile upon you."

## TempleShaligar.
class TempleShaligar(BaseTemple):
	name = "Shaligar"
	desc = "I am a servant of King of the Lava and the God of Flame, Shaligar.\nIf you would like to join our Temple, please touch the altar and Shaligar will smile upon you."
	enemy_name = "Drolaxi"
	enemy_desc = "Ah, the weak and cowardly sister of the Flame Lord.  One day, she shall no longer be able to keep our flames from consuming all things and our flames shall make all subjects to our will."

## Tabernacle.
class TempleTabernacle(BaseTemple):
	name = "Tabernacle"
	desc = "I am a servant of the God of Light and King of the Angels, Tabernacle.\nIf you would like to join our Church, please touch the altar and Tabernacle will smile upon you."
	enemy_name = "Rashindel"
	enemy_desc = "Caution child, for you speak of the Fallen One.  In the days before the worlds were created by our Lord Tabernacle, the archangel Rashindel stood at his right hand.  In that day, however, Rashindel sought to claim the throne of Heaven and unseat the Mighty Tabernacle.  The Demon King was quickly defeated and banished to Hell with the angels he managed to deceive and they were transformed into the awful demons and devils which threaten the lands to this day."

## Terria.
class TempleTerria(BaseTemple):
	name = "Terria"
	desc = "I am a servant of Mother Earth and the Goddess of Life, Terria.\nIf you would like to join our Temple, please touch the altar and Terria will smile upon you."
	enemy_name = "Moroch"
	enemy_desc = "Speak not of the Dark Lord here!  The King of Death with his awful necromantic minions that rise from the sleep of death are not to be trifled with, for they are dangerous.  Our Lady has long sought to remove the plague of death from the lands after that foul Lich ascended."

## Tylowyn.
class TempleTylowyn(BaseTemple):
	name = "Tylowyn"
	desc = "I am a servant of the first Queen of Elven Kind and Elven Goddess of Luck, Tylowyn.\nIf you would like to join our Temple, please touch the altar and Tylowyn will smile upon you."
	enemy_name = "Dalosha"
	enemy_desc = "That rebellious heretic!  In the days of the First Elven Kings, the first daughter of our gracious Tylowyn sought to overthrow the Elven Kingdoms with her lies and treachery.  After she was routed from the Elven lands, she took her band of rebel dark elves and hid in the caves, but unfortunately managed to survive there.  Avoid those drow if you know what is best for you."

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
		me.SayTo(activator, "\nWelcome to the church of <a>{0}</a>.".format(temple.name))

		# We don't like enemy gods.
		if temple.is_enemy_god():
			me.SayTo(activator, "\n... I see that you are a follower of <a>{0}</a>. It would be best for you to leave immediately.".format(temple.enemy_name), True)
			return True

		# Custom message to say before services.
		if temple.hello_msg:
			me.SayTo(activator, temple.hello_msg, True)

		# Sort the services.
		services = sorted(temple.services, key = lambda key: temple.services[key][2], reverse = True)

		if services:
			me.SayTo(activator, "I can offer you the following services:\n", True)

			for service in services:
				me.SayTo(activator, "<a>{0}</a> for {1}".format(service.capitalize(), temple.services[service][1] and CostString(temple.services[service][1]) or "free"), True)

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

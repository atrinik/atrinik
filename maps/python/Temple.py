## @file
## Generic code for implementing temples.

from Atrinik import *

## A base temple object.
class BaseTemple:
	## Name of the enemy god.
	_enemy_name = None
	## Description about the enemy god that will be shown when asked about
	## the enemy god.
	_enemy_desc = None

	## The initializer.
	## @param activator Who activated the script.
	## @param me The event owner.
	## @param inf Interface to use.
	def __init__(self, activator, me, inf):
		self.hello_msg = None

		self._activator = activator
		self._me = me
		self._inf = inf

		# Initialize default services.
		#
		# List entry meanings:
		# - If True this is a spell and can be automatically handled,
		#   otherwise you need a special action in handle_chat().
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

	## Check whether activator is of an enemy god.
	def is_enemy_god(self):
		# No enemy god, can't be an enemy.
		if not self._enemy_name:
			return False

		return self._activator.GetGod() == self._enemy_name

	## Handle chat.
	## @param msg The message that activated the chat.
	## @return True if the chat was handled, False otherwise.
	def handle_chat(self, msg):
		if msg == "hello":
			self._inf.add_msg("Welcome to the church of <a>{}</a>.".format(self._name))

			# We don't like enemy gods.
			if self.is_enemy_god():
				self._inf.add_msg("... I see that you are a follower of <a>{}</a>. It would be best for you to leave immediately.".format(self._enemy_name))
				return True

			# Custom message to say before services.
			if self.hello_msg:
				self._inf.add_msg(self.hello_msg)

			# Sort the services.
			services = sorted(self.services, key = lambda key: self.services[key][2], reverse = True)

			if services:
				self._inf.add_msg("I can offer you the following services:")

				for service in services:
					self._inf.add_link(service.capitalize())

			return True
		# Explain about the temple's god.
		elif msg == self._name.lower():
			self._inf.add_msg(self._desc)
			return True
		# Explain about the temple's enemy god, if there is one.
		elif self._enemy_name and msg == self._enemy_name.lower():
			self._inf.add_msg(self._enemy_desc)
			return True

		# No services for followers of enemy god.
		if self.is_enemy_god():
			return False

		is_buy = False

		if msg.startswith("buy "):
			msg = msg[4:]
			is_buy = True

		service = self.services.get(msg)

		# No such service...
		if not service:
			return False

		if service[0]:
			if not is_buy:
				self._inf.add_msg("{} will cost you {}. Is that okay?".format(msg.title(), CostString(service[1])))
				self._inf.add_link("<a=:buy {}>Yes.</a>".format(msg))
				self._inf.add_link("<a=close:>No thanks.</a>")
			else:
				if self._activator.PayAmount(service[1]):
					if service[1]:
						self._inf.add_msg("You pay {}.".format(CostString(service[1])), COLOR_YELLOW)

					self._inf.add_msg("Okay, I will cast {} on you now.".format(msg))
					self._me.Cast(GetSpellNr(msg), self._activator)
				else:
					self._inf.add_msg("You do not have enough money...")
		else:
			if msg == "food":
				if self._activator.food < 500:
					self._activator.food = 500
					self._inf.add_msg("Your stomach is filled again.")
				else:
					self._inf.add_msg("You don't look very hungry to me...")

		return True

## Grunhilde.
class TempleGrunhilde(BaseTemple):
	_name = "Grunhilde"
	_desc = "I am a servant of the Valkyrie Queen and the Goddess of Victory, Grunhilde.\nIf you would like to join our Temple and fight for our cause, please touch the altar and Grunhilde will smile upon you."

## Dalosha.
class TempleDalosha(BaseTemple):
	_name = "Dalosha"
	_desc = "I am a servant of the first Queen of the Drow and Spider Goddess, Dalosha.\nIf you would like to join our Temple, please touch the altar and Dalosha will smile upon you."
	_enemy_name = "Tylowyn"
	_enemy_desc = "The high elves and their oppressive queen!  Do not be swayed by her traps, she started the war with her attempt to enforce proper elven conduct in war.  Tylowyn was too cowardly and weak to realize that it was our destiny to rule the world, so now she and her elves shall also perish!"

## Drolaxi.
class TempleDrolaxi(BaseTemple):
	_name = "Drolaxi"
	_desc = "I am a servant of Queen of the Chaotic Seas and the Goddess of Water, Drolaxi.\nIf you would like to join our Temple, please touch the altar and Drolaxi will smile upon you."
	_enemy_name = "Shaligar"
	_enemy_desc = "Flames and terror does he seek to spread.  Do not be deceived, although the flame be kin to the Lady, he is complerely mad.  Avoid the scorching flames or they will consume you.  We shall rule the world and all shall be seas!"

## Grumthar.
class TempleGrumthar(BaseTemple):
	_name = "Grumthar"
	_desc = "I am a servant of the First Dwarven Lord and the God of Smithery, Grumthar.\nIf you would like to join our Temple, please touch the altar and Grumthar will smile upon you."
	_enemy_name = "Jotarl"
	_enemy_desc = "Do not be speaking of that Giant tyrant amongst us.  Him and his giants have long sought to crush the little folk.  He has those goblin vermin under his wing also."

## Jotarl.
class TempleJotarl(BaseTemple):
	_name = "Jotarl"
	_desc = "I am a servant of the Titan King and the God of the Giants, Jotarl.\nIf you would like to join our Temple, please touch the altar and Jotarl will smile upon you."
	_enemy_name = "Grumthar"
	_enemy_desc = "Puny dwarves do not scare Jotarl with their technology and mithril weapons, we shall rule the caves!  The Dwarves shall fall and we shall claim their gold for ourselves."

## Moroch.
class TempleMoroch(BaseTemple):
	_name = "Moroch"
	_desc = "I am a servant of the Lord of the Grave and King of Undeath, Moroch.\nIf you would like to join our Temple, please touch the altar and Moroch will smile upon you."
	_enemy_name = "Terria"
	_enemy_desc = "Do you honestly believe the lies of those naturists?  The powers of undeath will rule the universe and the servants of Nature will fail.  The Dark Lord shall not fail to dominate the land and all be consumed in glorious Death."

## Rashindel.
class TempleRashindel(BaseTemple):
	_name = "Rashindel"
	_desc = "I am a servant of the Demonic King and the Overlord of Hell, Rashindel.\nIf you would like to join our Circle, please touch the altar and Rashindel will smile upon you."
	_enemy_name = "Tabernacle"
	_enemy_desc = "Accursed fool, do not mention that name in our presence!  In the days before this world, the Tyrant sought to oppress us with the his oppressive ideals of truth and justice.  After our master freed us from the simpleton lots who follow him, he was bound into the darkness which is now our glorious kingdom."

## Rogroth.
class TempleRogroth(BaseTemple):
	_name = "Rogroth"
	_desc = "I am a servant of the King of the Stormy Skies and the God of Lightning, Rogroth.\nIf you would like to join our Temple, please touch the altar and Rogroth will smile upon you."

## TempleShaligar.
class TempleShaligar(BaseTemple):
	_name = "Shaligar"
	_desc = "I am a servant of King of the Lava and the God of Flame, Shaligar.\nIf you would like to join our Temple, please touch the altar and Shaligar will smile upon you."
	_enemy_name = "Drolaxi"
	_enemy_desc = "Ah, the weak and cowardly sister of the Flame Lord.  One day, she shall no longer be able to keep our flames from consuming all things and our flames shall make all subjects to our will."

## Tabernacle.
class TempleTabernacle(BaseTemple):
	_name = "Tabernacle"
	_desc = "I am a servant of the God of Light and King of the Angels, Tabernacle.\nIf you would like to join our Church, please touch the altar and Tabernacle will smile upon you."
	_enemy_name = "Rashindel"
	_enemy_desc = "Caution child, for you speak of the Fallen One.  In the days before the worlds were created by our Lord Tabernacle, the archangel Rashindel stood at his right hand.  In that day, however, Rashindel sought to claim the throne of Heaven and unseat the Mighty Tabernacle.  The Demon King was quickly defeated and banished to Hell with the angels he managed to deceive and they were transformed into the awful demons and devils which threaten the lands to this day."

## Terria.
class TempleTerria(BaseTemple):
	_name = "Terria"
	_desc = "I am a servant of Mother Earth and the Goddess of Life, Terria.\nIf you would like to join our Temple, please touch the altar and Terria will smile upon you."
	_enemy_name = "Moroch"
	_enemy_desc = "Speak not of the Dark Lord here!  The King of Death with his awful necromantic minions that rise from the sleep of death are not to be trifled with, for they are dangerous.  Our Lady has long sought to remove the plague of death from the lands after that foul Lich ascended."

## Tylowyn.
class TempleTylowyn(BaseTemple):
	_name = "Tylowyn"
	_desc = "I am a servant of the first Queen of Elven Kind and Elven Goddess of Luck, Tylowyn.\nIf you would like to join our Temple, please touch the altar and Tylowyn will smile upon you."
	_enemy_name = "Dalosha"
	_enemy_desc = "That rebellious heretic!  In the days of the First Elven Kings, the first daughter of our gracious Tylowyn sought to overthrow the Elven Kingdoms with her lies and treachery.  After she was routed from the Elven lands, she took her band of rebel dark elves and hid in the caves, but unfortunately managed to survive there.  Avoid those drow if you know what is best for you."

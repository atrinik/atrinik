## @file
## Gashir the bartender.

from Atrinik import *
from QuestManager import QuestManager
from Quests import ShipmentOfCharobBeer as quest
from Tavern import Bartender

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()
qm = QuestManager(activator, quest)

## Implements Gashir the bartender.
class BartenderGashir(Bartender):
	def _chat_greeting(self):
		if qm.finished():
			self.me.Communicate("/smile")
			self.me.SayTo(self.activator, "\nFinally, I get my shipment of Charob Beer!")
			self.activator.Write("You give the {0} to {1}.".format(quest["item_name"], self.me.name), COLOR_WHITE)
			self.me.SayTo(self.activator, "Thank you! Now I can serve you with <a>Charob Beer</a>. I am sure you'll get a nice reward for your delivery too!", 1)
			qm.complete()
		else:
			self.me.SayTo(self.activator, "\nWelcome to Asterian Arms Tavern.\nI'm Gashir, the bartender of this tavern. Here is the place if you want to eat or drink the best booze! I can offer you the following <a>provisions</a>:\n\n{}\n\nThe <a>Charob beer</a> is our specialty, and the large booze is really strong, so please do not <a>complain</a> about its quality.".format("\n".join(self._get_provisions())))

	def _can_buy(self, name):
		if name == "charob beer" and not qm.completed():
			return False

		return True

	def _chat_cannot_buy(self, name):
		if name == "charob beer":
			self.me.SayTo(self.activator, "\nAh, sorry. We are fresh out! Maybe you could check down at the brewery to see what is holding my shipment up?")

	def _chat_buy(self, num, obj):
		arch_name = obj.arch.name

		self.me.SayTo(self.activator, "\nHere you go, {} {}!".format(num, obj.name))

		if arch_name == "booze_generic":
			self.me.SayTo(self.activator, "Enjoy!", True)
		elif arch_name == "booze2":
			self.me.SayTo(self.activator, "Please be careful though, it is really strong!", True)
		elif arch_name == "food_generic":
			self.me.SayTo(self.activator, "It's really tasty, I tell you.", True)
		elif arch_name == "drink_generic":
			self.me.SayTo(self.activator, "Thirsty? Nothing like fresh water!", True)
		elif arch_name == "beer_charob":
			self.me.SayTo(self.activator, "It is quite good quality!", True)

def main():
	if msg == "complain":
		me.SayTo(activator, "\nI told you it is really strong! What did you expect? Elvish wine?")
	elif msg == "this ale stinks":
		me.SayTo(activator, "\nIf you don't like it, find another tavern!")
	else:
		bartender = BartenderGashir(activator, me, WhatIsEvent())
		bartender.handle_msg(msg)

main()

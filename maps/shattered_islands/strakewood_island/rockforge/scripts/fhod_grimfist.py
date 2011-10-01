## @file
## Script for Fhod Grimfist in Rockforge.

from Interface import Interface
from Tavern import Bartender

inf = Interface(activator, me)

class FhodGrimfist(Bartender):
	def _chat_greeting(self):
		self._inf.add_msg("*grumbles*", COLOR_YELLOW)
		self._inf.add_msg("Another customer... Very well... I am {}. What do you need? Make it quick, though.".format(self._me.name))
		self._create_provisions()

	def _chat_buy1(self, obj):
		self._inf.add_msg("Very well...")
		self._inf.add_msg_icon(obj.face[0], obj.GetName() + " for " + CostString(obj.value))
		self._inf.add_msg("How many do you want?")
		self._create_provision_numbers(obj)

	def _chat_buy2_success(self, obj):
		self._inf.add_msg("There you go.")
		self._inf.add_msg_icon(obj.face[0], obj.GetName())
		self._inf.add_msg("Now off with ya.")

	def _chat_buy2_fail(self):
		self._inf.add_msg("What's this, you don't have enough money?")

def main():
	bartender = FhodGrimfist(activator, me, WhatIsEvent(), inf)
	bartender.handle_msg(msg)

main()
inf.finish()

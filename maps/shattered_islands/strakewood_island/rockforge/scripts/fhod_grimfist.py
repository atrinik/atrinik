## @file
## Script for Fhod Grimfist in Rockforge.

from Interface import InterfaceBuilder
from Tavern import Bartender

class InterfaceDialog(Bartender, InterfaceBuilder):
    """
    Dialog when talking to the bartender.
    """

    def dialog_hello(self):
        self.add_msg("*grumbles*", color = COLOR_YELLOW)
        self.add_msg("Another customer... Very well... I am {npc.name}. What do you need? Make it quick, though.")
        self.show_provisions()

    def show_buy(self, obj):
        self.add_msg("Very well...")
        self.show_buy_icon(obj)
        self.add_msg("How many do you want?")
        self.show_provision(obj)

    def show_bought(self, obj):
        self.add_msg("There you go.")
        self.show_buy_icon(obj, buying = False)
        self.add_msg("Now off with ya.")

    def show_fail_weight(self):
        self.add_msg("What's this, you're not strong enough to carry that?")

    def show_fail_money(self):
        self.add_msg("What's this, you don't have enough money?")

ib = InterfaceDialog(activator, me)
ib.finish(msg)

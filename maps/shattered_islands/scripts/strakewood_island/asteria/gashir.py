## @file
## Gashir the bartender.

from Interface import Interface
from QuestManager import QuestManagerMulti
from Quests import ShipmentOfCharobBeer as quest
from Tavern import Bartender

inf = Interface(activator, me)
qm = QuestManagerMulti(activator, quest)

## Implements Gashir the bartender.
class BartenderGashir(Bartender):
    def _chat_greeting(self):
        self._inf.add_msg("Welcome to Asterian Arms Tavern.")
        self._inf.add_msg("I'm {}, the bartender of this tavern. Here is the place if you want to eat or drink the best booze! I can offer you the following provisions.".format(self._me.name))

        if not qm.completed_part(1):
            self._inf.add_msg("Unfortunately, it appears we are fresh out of our local specialty, the Charob Beer. If you want to buy some, you'll have to wait until the shipment arrives. In the meantime, maybe you could check down at the brewery to see what is holding my shipment up...")

            if qm.finished(1):
                self._inf.add_link("I have your shipment of Charob Beer.", dest = "shipment")

        self._create_provisions()

    def _can_buy(self, obj):
        if obj.arch.name == "beer_charob" and not qm.completed_part(1):
            return False

        return True

    def _chat_buy2_success(self, obj):
        arch_name = obj.arch.name

        self._inf.add_msg("Here you go!")
        self._inf.add_msg_icon(obj.face[0], obj.GetName() + " for " + CostString(obj.value))

        if arch_name == "booze_generic":
            self._inf.add_msg("Enjoy!")
        elif arch_name == "booze2":
            self._inf.add_msg("Please be careful though, it is really strong!")
        elif arch_name == "food_generic":
            self._inf.add_msg("It's really tasty, I tell you.")
        elif arch_name == "drink_generic":
            self._inf.add_msg("Thirsty? Nothing like fresh water!")
        elif arch_name == "beer_charob":
            self._inf.add_msg("It is quite good quality!")

def main():
    if msg == "shipment":
        if qm.started_part(1) and qm.finished(1):
            inf.add_msg("Finally, I get my shipment of Charob Beer!")
            inf.add_msg("You hand over the shipment of the Charob Beer.", COLOR_YELLOW)
            inf.add_msg("Thank you! Now I can serve you with Charob Beer. I am sure you'll get a payment for your delivery too!")
            qm.start(2)
            qm.complete(1, sound = None)
    else:
        bartender = BartenderGashir(activator, me, WhatIsEvent(), inf)
        bartender.handle_msg(msg)

main()
inf.finish()

## @file
## API for handling taverns.

from Atrinik import *
from Language import int2english
import re

class Bartender:
    max_provisions = 100

    def find_provision(self, name):
        for obj in WhatIsEvent().inv:
            if obj.GetName() == name and self.can_buy(obj):
                return obj

        return None

    def show_provision(self, obj):
        name = obj.GetName()

        for x in [1, 5, 10, 25, 50, 100, 500, 1000]:
            if x > self.max_provisions:
                break

            self.add_link(int2english(x).capitalize(), dest = "buy {} {}".format(x, name))

    def show_provisions(self):
        for obj in sorted(WhatIsEvent().inv, key = lambda obj: obj.name):
            if not self.can_buy(obj):
                continue

            name = obj.GetName()
            self.add_link(name.capitalize(), dest = "buy " + name)

    def show_buy(self, obj):
        self.add_msg("Ah, excellent choice!")
        self.add_msg_icon(obj.face[0], obj.GetName() + " for " + CostString(obj.value))
        self.add_msg("How many do you want to purchase?")
        self.show_provision(obj)

    def show_bought(self, obj):
        self.add_msg("You pay {}.".format(CostString(obj.value * obj.nrof)), color = COLOR_YELLOW)
        self.add_msg("Here you go!")
        self.add_msg_icon(obj.face[0], obj.GetName())
        self.add_msg(obj.msg or "Pleasure doing business with you!")

    def show_fail_weight(self):
        self.add_msg("You can't carry that much weight...")

    def show_fail_money(self):
        self.add_msg("Sorry, you don't have enough money...")

    def can_buy(self, obj):
        return True

    def dialog_hello(self):
        self.add_msg("Welcome to my tavern, dear customer! I am {npc.name}, the bartender.")
        self.add_msg("I can offer you the following provisions.")
        self.show_provisions()

    def dialog(self, msg):
        match = re.match(r"buy (\d+)?\s*(.*)", msg)

        if not match:
            return

        num, name = match.groups()
        obj = self.find_provision(name)

        if not obj:
            return

        if num != None:
            num = max(1, min(int(num), self.max_provisions))

            if not self._activator.Controller().CanCarry(obj.weight * num + obj.carrying):
                self.show_fail_weight()
            elif not self._activator.PayAmount(obj.value * num):
                self.show_fail_money()
            else:
                clone = obj.Clone()
                # Adjust the nrof.
                clone.nrof = num
                self.show_bought(clone)

                # Reset value and msg to arch default so the provisions stack
                # properly with regular food drops.
                clone.value = clone.arch.clone.value
                clone.msg = clone.arch.clone.msg

                # Insert the clone into the player.
                clone.InsertInto(self._activator)
        else:
            self.show_buy(obj)

## @file
## API for handling taverns.

from Atrinik import *

## Implements tavern bartenders.
class Bartender:
    ## Initialize the bartender class.
    ## @param activator Script activator.
    ## @param me Bartender NPC.
    ## @param event Event object.
    ## @param inf The interface to use.
    def __init__(self, activator, me, event, inf):
        self._activator = activator
        self._me = me
        self._event = event
        self._inf = inf

        # Maximum number of provisions.
        self.max_provisions = 100

    ## Check if the specified provision can be bought.
    ## @param obj The provision to check.
    def _can_buy(self, obj):
        return True

    ## Creates the provision links.
    def _create_provisions(self):
        for obj in sorted(self._event.inv, key = lambda obj: obj.name):
            if not self._can_buy(obj):
                continue

            name = obj.GetName()
            self._inf.add_link(name.capitalize(), dest = "buy1 " + name)

    ## Creates the links when NPC asks the player how many of the
    ## specified provision to buy.
    ## @param obj The provision.
    def _create_provision_numbers(self, obj):
        from Language import int2english

        name = obj.GetName()

        for x in [1, 5, 10, 25, 50, 100, 500, 1000]:
            if x > self.max_provisions:
                break

            self._inf.add_link(int2english(x).capitalize(), dest = "buy2 " + str(x) + " " + name)

    ## Main dialog.
    def _chat_greeting(self):
        self._inf.add_msg("Welcome to my tavern, dear customer! I am {}, the bartender.".format(self._me.name))
        self._inf.add_msg("I can offer you the following provisions.")
        self._create_provisions()

    ## Dialog when the player chooses a provision to buy.
    ## @param obj The provision the player has chosen.
    def _chat_buy1(self, obj):
        self._inf.add_msg("Ah, excellent choice!")
        self._inf.add_msg_icon(obj.face[0], obj.GetName() + " for " + CostString(obj.value))
        self._inf.add_msg("How many do you want to purchase?")
        self._create_provision_numbers(obj)

    ## Message to display when the player has confirmed the number of
    ## provisions to buy, but cannot carry them all.
    def _chat_buy2_weight(self):
        self._inf.add_msg("You can't carry that much weight...")

    ## Message to display when the player has successfully purchased
    ## provision(s).
    ## @param obj The provision(s) the player will receive.
    def _chat_buy2_success(self, obj):
        self._inf.add_msg("Here you go!")
        self._inf.add_msg_icon(obj.face[0], obj.GetName())
        self._inf.add_msg("Pleasure doing business with you!")

    ## Message to display when the player has confirmed the number of
    ## provisions to buy, but doesn't have enough money.
    def _chat_buy2_fail(self):
        self._inf.add_msg("Sorry, you don't have enough money...")

    ## Handle a message.
    ## @param msg Message to handle.
    ## @return True if the message was handled, False otherwise.
    def handle_msg(self, msg):
        if msg == "hello":
            self._chat_greeting()
            return True
        elif msg.startswith("buy1 "):
            name = msg[5:]

            for obj in self._event.inv:
                if obj.GetName() == name:
                    if not self._can_buy(obj):
                        break

                    self._chat_buy1(obj)
                    break

            return True
        elif msg.startswith("buy2 "):
            import re

            # Attempt to get the number of the provisions and the provision name.
            match = re.match(r"(\d+) (.*)", msg[5:])

            if not match:
                return True

            # Acquire the number and the name.
            (num, name) = match.groups()
            # Make sure the number does not overflow.
            num = max(1, min(int(num), self.max_provisions))

            for obj in self._event.inv:
                if obj.GetName() == name:
                    if not self._can_buy(obj):
                        break

                    # See if the player can carry the provision(s).
                    if not self._activator.Controller().CanCarry(obj.weight * num + obj.carrying):
                        self._chat_buy2_weight()
                    # Pay the amount...
                    elif self._activator.PayAmount(obj.value * num):
                        self._inf.add_msg("You pay {}.".format(CostString(obj.value * num)), COLOR_YELLOW)
                        clone = obj.Clone()
                        # Adjust the nrof.
                        clone.nrof = num
                        # Reset value to arch default so the provisions stack properly
                        # with regular food drops.
                        clone.value = clone.arch.clone.value
                        self._chat_buy2_success(clone)
                        # Insert the clone into the player.
                        clone.InsertInto(self._activator)
                    # Can't afford it!
                    else:
                        self._chat_buy2_fail()

            return True

        return False

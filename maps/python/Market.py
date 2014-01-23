## @file
## Market related functions.

from Atrinik import *

## Merchant API.
class Merchant:
    ## Initialize the merchant API.
    ## @param activator The script activator.
    ## @param me The merchant.
    ## @param event The event.
    ## @param inf The interface to use.
    def __init__(self, activator, me, event, inf):
        self._activator = activator
        self._me = me
        self._inf = inf
        self._event = event

    ## Handle a message.
    ## @param msg Message to handle.
    ## @return True if the message was handled, False otherwise.
    def handle_msg(self, msg):
        if msg == "hello":
            self._inf.add_msg("Welcome, dear customer!")

            # Message in event, create treasure.
            if self._event.msg:
                import json

                # Parse data, create the treasure.
                for (treasure, num, a_chance) in json.loads(self._event.msg):
                    for i in range(num):
                        self._event.CreateTreasure(treasure, self._me.level, GT_ONLY_GOOD, a_chance)

                self._event.msg = None

            if self._event.inv and not self._event.inv.ReadKey("stock_nrof"):
                # Make sure all objects in the merchant's inventory have
                # stock_nrof attribute, which stores the actual number of
                # items left.
                for obj in self._event.inv:
                    obj.f_identified = True
                    obj.WriteKey("stock_nrof", str(max(1, obj.nrof)))
                    obj.nrof = 1

            found_goods = False

            for obj in sorted(self._event.inv, key = lambda obj: obj.GetName()):
                if obj.ReadKey("stock_nrof") == "0":
                    continue

                name = obj.GetName()
                self._inf.add_link(name.capitalize(), dest = "buy1 " + name)
                found_goods = True

            if not found_goods:
                self._inf.add_msg("Sorry, it seems I am out of stock! Please come back later.")
            else:
                self._inf.add_msg("I can offer you the following.")

            return True
        elif msg.startswith("buy1 "):
            name = msg[5:]

            for obj in self._event.inv:
                if obj.GetName() == name:
                    stock_nrof = int(obj.ReadKey("stock_nrof"))

                    # No more goods left.
                    if stock_nrof <= 0:
                        self._inf.add_msg("Oh my, it seems we don't have any {} left... Please come back later.".format(name))
                        break

                    self._inf.add_msg("Splendid choice!")
                    self._inf.add_msg_icon(obj.face[0], name + " for " + CostString(obj.GetCost(obj, COST_TRUE)))
                    self._inf.add_msg("How many do you want to purchase?")

                    from Language import int2english

                    for x in [1, 5, 10, 25, 50, 100, 500, 1000]:
                        if x > stock_nrof:
                            break

                        self._inf.add_link(int2english(x).capitalize(), dest = "buy2 " + str(x) + " " + name)

                    break

            return True
        elif msg.startswith("buy2 "):
            import re

            # Attempt to get the number of the items to buy and the item name.
            match = re.match(r"(\d+) (.*)", msg[5:])

            if not match:
                return True

            # Acquire the number and the name.
            (num, name) = match.groups()

            for obj in self._event.inv:
                if obj.GetName() == name:
                    stock_nrof = int(obj.ReadKey("stock_nrof"))

                    # No more goods left.
                    if stock_nrof <= 0:
                        self._inf.add_msg("Oh my, it seems we don't have any {} left... Please come back later.".format(name))
                        break

                    # Make sure the number does not overflow.
                    num = max(1, min(int(num), stock_nrof))
                    cost = obj.GetCost(obj, COST_TRUE) * num

                    if not self._activator.PayAmount(cost):
                        self._inf.add_msg("Sorry, you don't have enough money...")
                        break

                    self._inf.add_msg("You pay {}.".format(CostString(cost)), COLOR_YELLOW)

                    clone = obj.Clone()
                    clone.WriteKey("stock_nrof")
                    clone.nrof = num
                    self._inf.add_msg_icon(clone.face[0], clone.GetName())
                    clone.InsertInto(self._activator)

                    self._inf.add_msg("Thank you, pleasure doing business with you!")

                    obj.WriteKey("stock_nrof", str(stock_nrof - num))

                    break

            return True

        return False

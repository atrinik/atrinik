"""Seller.py: Implements the Seller class."""

from Atrinik import *
from Interface import InterfaceBuilder
from Language import int2english
import json
import random
import re

class Merchant(InterfaceBuilder):
    _goods = None
    max_goods = 100

    @property
    def goods(self):
        raise NotImplementedError("not implemented")

    def find_goods(self, name):
        for obj in self.goods:
            if obj.GetName() == name:
                return obj

        return None

    def generate_item_links(self, dest, item, nrof):
        name = item.GetName()

        for x in [1, 5, 10, 25, 50, 100, 500, 1000]:
            if x > self.max_goods or (nrof and x > nrof):
                break

            self.add_link(int2english(x).capitalize(), dest = dest + "{} {}".format(x, name))

class Seller(Merchant):
    """Used for NPCs that sell something to the player."""

    max_goods = 100

    @property
    def goods(self):
        if self._goods is not None:
            return self._goods

        event = WhatIsEvent()

        # Message in event, create treasure.
        if event.msg:
            # Parse data, create the treasure.
            for (treasure, num, a_chance) in json.loads(event.msg):
                if treasure:
                    for i in range(num):
                        event.CreateTreasure(treasure, self._npc.level, GT_ONLY_GOOD, a_chance)
                else:
                    for obj in event.inv:
                        obj.nrof = num

                        if a_chance:
                            obj.nrof += random.randint(1, a_chance)

            event.msg = None

        if event.ReadKey("stock") == "1":
            for obj in event.inv:
                obj.f_identified = True
                obj.WriteKey("stock_nrof", str(max(1, obj.nrof)))
                obj.nrof = 1

            event.WriteKey("stock", "0")

        self._goods = []

        for obj in sorted(event.inv, key = lambda obj: obj.GetName()):
            if obj.ReadKey("stock_nrof") == "0":
                continue

            if not self.can_buy_goods(obj):
                continue

            self._goods.append(obj)

        return self._goods

    def can_buy_goods(self, obj):
        fnc = getattr(self, "subdialog_can_buy_" + obj.arch.name, None)

        if fnc:
            return fnc()

        return True

    def dialog_hello(self):
        self.add_msg("Welcome, dear customer!")
        self.subdialog_goods()

    def subdialog_stockout(self):
        self.add_msg("Sorry, it seems I am out of stock! Please come back later.")

    def subdialog_stockoutitem(self):
        self.add_msg("Sorry, it seems I am out of stock for {self.buy_item.name}! Please come back later.")

    def subdialog_stockin(self):
        self.add_msg("I can offer you the following.")

    def subdialog_stock(self):
        for obj in self.goods:
            name = obj.GetName()
            self.add_link(name.capitalize(), dest = "buy " + name)

    def subdialog_stockitem(self):
        self.generate_item_links("buy ", self.buy_item, self.buy_item_stock)

    def subdialog_goods(self):
        if not self.goods:
            self.subdialog_stockout()
        else:
            self.subdialog_stockin()
            self.subdialog_stock()

    def subdialog_buy(self):
        cost = CostString(self.buy_item.GetCost(self.buy_item, COST_TRUE))
        desc = "{} for {}".format(self.buy_item.GetName(), cost)
        self.add_msg_icon_object(self.buy_item, desc=desc)

    def subdialog_bought(self):
        self.add_msg_icon_object(self.buy_item)

    def subdialog_buyitem(self):
        self.add_msg("Splendid choice!")
        self.subdialog_buy()
        self.add_msg("How many do you want to purchase?")
        self.subdialog_stockitem()

    def subdialog_boughtitem(self):
        self.subdialog_bought()
        self.add_msg("Pleasure doing business with you!")

    def subdialog_fail_weight(self):
        self.add_msg("You can't carry that much weight...")

    def subdialog_fail_money(self):
        self.add_msg("Sorry, you don't have enough money...")

    def dialog(self, msg):
        match = re.match(r"buy (\d+)?\s*(.*)", msg)

        if not match:
            return

        num, name = match.groups()
        obj = self.find_goods(name)

        if not obj or not self.can_buy_goods(obj):
            return

        obj_stock = obj.ReadKey("stock_nrof")

        if obj_stock != None:
            obj_stock = int(obj_stock)

            # No more goods left.
            if obj_stock <= 0:
                self.subdialog_stockout()
                return

        # Store these in the class object so that we can access them from
        # the sub-dialogues.
        self.buy_item = obj
        self.buy_item_stock = obj_stock

        if num != None:
            num = max(1, min(int(num), self.max_goods))

            if obj_stock != None:
                num = min(num, obj_stock)

            cost = obj.GetCost(obj, COST_TRUE) * num

            if not self._activator.Controller().CanCarry(obj.weight * num + obj.carrying):
                self.subdialog_fail_weight()
            elif not self._activator.PayAmount(cost):
                self.subdialog_fail_money()
            else:
                clone = obj.Clone()
                clone.nrof = num
                clone.WriteKey("stock_nrof")

                # Reset value to arch default so the goods stack
                # properly with regular items.
                clone.value = clone.arch.clone.value

                self.buy_item = clone

                self.add_msg("You pay {}.".format(CostString(cost)), color = COLOR_YELLOW)
                getattr(self, "subdialog_bought_" + obj.arch.name, self.subdialog_boughtitem)()

                # Insert the clone into the player.
                clone.InsertInto(self._activator)

                if obj_stock != None:
                    obj.WriteKey("stock_nrof", str(obj_stock - num))
        else:
            getattr(self, "subdialog_buy_" + obj.arch.name, self.subdialog_buyitem)()

class Buyer(Merchant):
    @property
    def goods(self):
        if self._goods is not None:
            return self._goods

        event = WhatIsEvent()
        self._goods = []

        for obj in sorted(event.inv, key = lambda obj: obj.GetName()):
            self._goods.append(obj)

        return self._goods

    def dialog_hello(self):
        self.add_msg("Welcome, dear customer!")
        self.add_msg("I would be interested in the following items.")
        self.subdialog_goods()

    def subdialog_goods(self):
        for obj in self.goods:
            name = obj.GetName()
            self.add_link(name.capitalize(), dest = "sell " + name)

    def subdialog_noitem(self):
        self.add_msg("Ah, excellent!")
        self.subdialog_sell()
        self.add_msg("Oh, you don't have any of those!")

    def subdialog_item(self):
        self.generate_item_links("sell ", self.sell_item, self.sell_item_nrof)

    def subdialog_sell(self):
        cost = CostString(self.sell_item.GetCost(self.sell_item, COST_TRUE))
        desc = "{} for {}".format(self.sell_item.GetName(), cost)
        self.add_msg_icon_object(self.sell_item, desc=desc)

    def subdialog_sellitem(self):
        self.add_msg("Ah, excellent!")
        self.subdialog_sell()
        self.add_msg("How many do you want to sell?")
        self.subdialog_item()

    def subdialog_sold(self):
        self.add_msg_icon_object(self.sell_item)

    def subdialog_solditem(self):
        self.subdialog_sold()
        self.add_msg("Pleasure doing business with you!")

    def dialog(self, msg):
        match = re.match(r"sell (\d+)?\s*(.*)", msg)

        if not match:
            return

        num, name = match.groups()
        obj = self.find_goods(name)

        if not obj:
            return

        item = self._activator.FindObject(archname=obj.arch.name, name=obj.name)
        self.sell_item = obj

        if not item:
            getattr(self, "subdialog_noitem_" + obj.arch.name, self.subdialog_noitem)()
            return

        self.sell_item_nrof = item.nrof

        if num != None:
            num = max(1, min(int(num), self.max_goods, item.nrof))
            cost = obj.GetCost(obj, COST_TRUE) * num

            item = item.Decrease(num)
            self._activator.Controller().InsertCoins(cost)

            self.add_msg("You receive {}.".format(CostString(cost)), color = COLOR_YELLOW)
            getattr(self, "subdialog_sold_" + obj.arch.name, self.subdialog_solditem)()
        else:
            getattr(self, "subdialog_sell_" + obj.arch.name, self.subdialog_sellitem)()

"""Seller.py: Implements the Seller class."""

from Atrinik import *
from Interface import InterfaceBuilder
from Language import int2english
import json
import re

class Seller(InterfaceBuilder):
    """Used for NPCs that sell something to the player."""

    max_goods = 100
    _goods = None

    @property
    def goods(self):
        if self._goods is not None:
            return self._goods

        event = WhatIsEvent()

        # Message in event, create treasure.
        if event.msg:
            # Parse data, create the treasure.
            for (treasure, num, a_chance) in json.loads(event.msg):
                for i in range(num):
                    obj = event.CreateTreasure(treasure, self._npc.level, GT_ONLY_GOOD, a_chance)

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

    def find_goods(self, name):
        for obj in self.goods:
            if obj.GetName() == name:
                return obj

        return None

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
        name = self.buy_item.GetName()

        for x in [1, 5, 10, 25, 50, 100, 500, 1000]:
            if x > self.max_goods or (self.buy_item_stock and x > self.buy_item_stock):
                break

            self.add_link(int2english(x).capitalize(), dest = "buy {} {}".format(x, name))

    def subdialog_goods(self):
        if not self.goods:
            self.subdialog_stockout()
        else:
            self.subdialog_stockin()
            self.subdialog_stock()

    def subdialog_buy(self):
        self.add_msg_icon(self.buy_item.face[0], self.buy_item.GetName() + " for " + CostString(self.buy_item.GetCost(self.buy_item, COST_TRUE)))

    def subdialog_bought(self):
        self.add_msg_icon(self.buy_item.face[0], self.buy_item.GetName())

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

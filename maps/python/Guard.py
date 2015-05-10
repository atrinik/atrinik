from Atrinik import *
from Interface import InterfaceBuilder
from Thieves import ThievesBountyEraser

class GuardStop(InterfaceBuilder):
    def get_bounty(self):
        return self._activator.Controller().FactionGetBounty(self._npc.ReadKey("faction"))

    @staticmethod
    def get_price(bounty):
        return int(ThievesBountyEraser.get_price(bounty) * 2.5)

    def dialog_hello(self):
        self.add_msg("Stop! You have violated the Law! Pay the court a fine or leave this place and never return!")
        self.add_link("I will pay my bounty.", dest="pay")
        self.add_link("<resist arrest>", action="close")

    def dialog(self, msg):
        bounty = self.get_bounty()
        cost = self.get_price(bounty)

        if msg == "pay":
            self.add_msg("Oh, I was hoping you would resist...")
            self.add_msg("Okay, my papers state that your bounty has been set to {cost}. Would you like to pay this amount and be cleared of your bounty?", cost=CostString(cost))
            self.add_link("Confirm.", dest="dopay")
            self.add_link("<resist arrest>", action="close")
        elif msg == "dopay":
            if self._activator.PayAmount(cost):
                self.add_msg("You pay {cost}.", cost=CostString(cost), color=COLOR_YELLOW)
                self.add_msg("Make sure to keep to the Law next time...")
                self._activator.Controller().FactionClearBounty(self._npc.ReadKey("faction"))
            else:
                self.add_msg("You do not have enough money...")
                self.add_link("<resist arrest>", action="close")

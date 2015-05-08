from Atrinik import *
from Interface import InterfaceBuilder

class ThievesBountyEraser(InterfaceBuilder):
    """Bounty eraser."""

    def get_bounties(self):
        d = dict()

        for faction in GetOptions().split(","):
            bounty = self._activator.Controller().FactionGetBounty(faction.strip())

            if bounty >= 0.0:
                continue

            d[faction] = bounty

        return d

    @staticmethod
    def get_price(bounty):
        return int(-bounty * 35.0)

    @staticmethod
    def get_faction_name(faction):
        return faction.replace("_", " ").title()

    def subdialog_services(self):
        """Show the available temple services."""

        for service in temple_services:
            self.add_link(temple_services[service][1], dest = service)

        self.add_link("Tell me about {}.".format(self.temple_name), dest = self.temple_name)

        if self.enemy_temple_name:
            self.add_link("Tell me about {}.".format(self.enemy_temple_name), dest = self.enemy_temple_name)

    def dialog_hello(self):
        """Default hello dialog handler."""

        bounties = self.get_bounties()

        if not bounties:
            self.add_msg("Hi there! I'm {npc.name}. If you ever have any problems with the guards, come to me and I may be able to help... for a price.")
        else:
            self.add_msg("Psst! If you need help with the guards, I can help with you with that:")

            for faction in bounties:
                self.add_link("Remove {bounty:0.2f} bounty from {faction}".format(bounty=-bounties[faction], faction=self.get_faction_name(faction)), dest=faction)

    def dialog(self, msg):
        """Handle removing the bounty."""

        if msg.startswith("remove "):
            msg = msg[7:]
            is_remove = True
        else:
            is_remove = False

        bounties = self.get_bounties()

        if msg not in bounties:
            return

        cost = self.get_price(bounties[msg])

        self.add_msg("[title]{faction}[/title]", faction=self.get_faction_name(msg))

        if not is_remove:
            self.add_msg("I will talk to the guards and convince them to drop the charges against you.")
            self.add_msg("This will cost you {cost}.", cost=CostString(cost))
            self.add_link("Confirm", dest="remove " + msg)
        else:
            if self._activator.PayAmount(cost):
                self.add_msg("You pay {cost}.", cost=CostString(cost), color=COLOR_YELLOW)
                self.add_msg("Consider it done. And try to stick to the shadows next time...")
                self._activator.Controller().FactionClearBounty(msg)
            else:
                self.add_msg("You do not have enough money...")

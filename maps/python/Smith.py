"""
Implements Smith related functions.
"""

from collections import OrderedDict

import Atrinik
from Interface import InterfaceBuilder


smith_services = OrderedDict((
    ("identify_all", (
        lambda npc: 200 + (50 * npc.level),
        "Identification of all objects",
    )),
    ("identify", (
        lambda npc: 50 + (10 * npc.level),
        "Identification of a single marked object",
    )),
))
"""
Contains all the possible smith services.
"""


class Smith(InterfaceBuilder):
    """
    Implements generic smith services.
    """

    def subdialog_services(self):
        """Show the available temple services."""

        for service in smith_services:
            self.add_link(smith_services[service][1], dest=service)

    def subdialog_service_identify_all(self):
        """Message for the mass identify service."""

        self.add_msg("I will identify all of the objects in your inventory, "
                     "including containers.")

    def subdialog_service_identify_all_marked_container(self):
        """
        Additional message for the mass identify service if the player has a
        marked container.
        """

        self.add_msg("Ah, you have a container marked! Alright then, I will "
                     "identify all of the objects in that container instead, "
                     "then.")

    def subdialog_service_identify(self):
        """Message for the single item identify service."""

        self.add_msg("I will identify a single marked object in your "
                     "inventory.")

    def subdialog_service_identify_no_marked(self):
        """Message when there's no marked object for the identify service."""
        self.add_msg("... but it seems you do not have a marked object.")

    def subdialog_fail_money(self):
        """Message when the player doesn't have enough money for the service."""

        self.add_msg("Sorry, you don't have enough money...")

    def subdialog_purchased_service(self):
        """Message after purchasing a service."""

        self.add_msg("Thank you for your business!")

    def dialog_hello(self):
        """Default hello dialog handler."""

        self.add_msg("Welcome! I am {npc.name}, the smith.")
        self.add_msg("I can offer you the following services.")
        self.subdialog_services()

    def dialog(self, msg):
        """Handle services."""

        if msg.startswith("buy "):
            msg = msg[4:]
            is_buy = True
        else:
            is_buy = False

        service = smith_services.get(msg)
        if not service:
            return

        marked = self._activator.Controller().FindMarkedObject()
        cost = service[0](self._npc)

        self.add_msg("[title]{service[1]}[/title]", service=service)

        if not is_buy:
            getattr(self, "subdialog_service_" + msg)()

            if msg == "identify_all":
                if marked and marked.type == Atrinik.Type.CONTAINER:
                    self.subdialog_service_identify_all_marked_container()
                    self.add_msg_icon_object(
                        marked, "Contents of the container will be identified"
                    )
            elif msg == "identify":
                if not marked:
                    self.subdialog_service_identify_no_marked()
                    return

                self.add_msg_icon_object(marked, "Will be identified")

            if cost != 0:
                self.add_msg("This will cost you {cost}.",
                             cost=Atrinik.CostString(cost))

            self.add_link("Confirm", dest="buy " + msg)
            return

        if msg == "identify" and not marked:
            return

        if cost != 0:
            if not self._activator.PayAmount(cost):
                self.subdialog_fail_money()
                return

            self.add_msg("You pay {cost}.", cost=Atrinik.CostString(cost),
                         color=Atrinik.COLOR_YELLOW)

        if msg == "identify_all":
            self._npc.CastIdentify(self._activator, Atrinik.IDENTIFY_ALL,
                                   marked)
        elif msg == "identify":
            self._npc.CastIdentify(self._activator, Atrinik.IDENTIFY_MARKED,
                                   marked)

        self.subdialog_purchased_service()

## @file
## Generic smith API.

from Atrinik import *

## The generic smith API.
class Smith:
    ## The initializer.
    ## @param activator Who activated the script.
    ## @param me The NPC.
    ## @param inf Interface to use.
    def __init__(self, activator, me, inf):
        self.hello_msg = None

        self._activator = activator
        self._me = me
        self._inf = inf

        self.services = {
            "idall": [
                200 + (50 * self._activator.level),
                50,
                "Identification of all objects",
            ],
            "identify": [
                50 + (10 * self._activator.level),
                0,
                "Identification of a single marked object",
            ],
        }

    ## Handle chat messages for the smith.
    ## @param msg The message to handle.
    ## @return True if the message was handled, False otherwise.
    def handle_chat(self, msg):
        if msg == "hello":
            self._inf.add_msg("Welcome! I am {}, the smith.".format(self._me.name))

            services = sorted(self.services, key = lambda key: self.services[key][1], reverse = True)

            if services:
                self._inf.add_msg("I can offer you the following services.")

                for service in services:
                    self._inf.add_link(self.services[service][2], dest = service)

            return True

        is_buy = False

        if msg.startswith("buy "):
            msg = msg[4:]
            is_buy = True

        service = self.services.get(msg)

        # No such service...
        if not service:
            return False

        # Find marked object; need it in most cases anyway;
        marked = self._activator.Controller().FindMarkedObject()
        # Add the title for what is being done.
        self._inf.add_msg("<title>{}</title>".format(service[2]))

        if not is_buy:
            # Explain about single-item identification
            if msg == "identify":
                self._inf.add_msg("I will identify a single marked object in your inventory.")

                if not marked:
                    self._inf.add_msg("... But it seems you do not have a marked object.")
                    return True

                self._inf.add_msg_icon(marked.face[0], "Will be identified")
            # Identification of all items.
            elif msg == "idall":
                self._inf.add_msg("I will identify all of the objects in your inventory, including containers.")

                if marked and marked.type == Type.CONTAINER:
                    self._inf.add_msg("Ah, you have a container marked! Alright then, I will identify all of the objects in that container instead, then.")
                    self._inf.add_msg_icon(marked.face[0], "Contents of the container will be identified")

            if service[0]:
                self._inf.add_msg("This will cost you {}.".format(CostString(service[0])))

            self._inf.add_link("Confirm".format(msg), dest = "buy " + msg)
        else:
            # Make sure we still have the marked object...
            if msg == "identify" and not marked:
                self._inf.add_msg("Hm? What did you want to identify again?")
                return True

            if self._activator.PayAmount(service[0]):
                if service[0]:
                    self._inf.add_msg("You pay {}.".format(CostString(service[0])), COLOR_YELLOW)

                self._inf.add_msg("Thank you for your business!")

                if msg == "idall":
                    self._me.CastIdentify(self._activator, IDENTIFY_ALL, marked)
                elif msg == "identify":
                    self._me.CastIdentify(self._activator, IDENTIFY_MARKED, marked)
            else:
                self._inf.add_msg("Sorry, you do not have enough money...")

        return True

## @file
## Implements generic bank system for NPCs.

from Atrinik import *

## The generic bank system used by bankers.
class Bank:
    ## Initialize the class.
    ## @param activator The activator.
    ## @param me The NPC.
    ## @param inf The interface.
    def __init__(self, activator, me, inf):
        self._activator = activator
        self._me = me
        self._inf = inf

    ## Add service link to the interface.
    def _service_links(self):
        self._inf.add_link("I'd like to make a deposit.", dest = "dodeposit")
        self._inf.add_link("I'd like to make a withdrawal.", dest = "dowithdraw")
        self._inf.add_link("I'd like to deposit all my money on hand.", dest = "deposit all")
        self._inf.add_link("I'd like to withdraw all money from my account.", dest = "withdraw all")
        self._inf.add_link("I'd like to see the balance of my account.", dest = "balance")

    ## Handle the chat.
    ## @param msg The message to handle.
    ## @return True if the message was handled, False otherwise.
    def handle_chat(self, msg):
        # Greeting.
        if msg == "hello":
            self._inf.add_msg("Welcome to our bank. My name is {}.".format(self._me.name))
            self._inf.add_msg("How can I serve you today?")
            self._service_links()
            self._inf.add_link("Can you explain the bank system to me?", dest = "explain")
            return True

        # Make a deposit.
        elif msg == "dodeposit":
            self._inf.add_msg("How much money would you like to deposit?")
            self._inf.add_msg("For example, type [green]1 gold, 99 silver, 20 copper[/green], or [green]50 copper[/green], or [green]all[/green].")
            self._inf.set_text_input(prepend = "deposit ")
            return True

        # Actually perform the deposit.
        elif msg.startswith("deposit "):
            (ret, value) = self._activator.Controller().BankDeposit(msg[8:])

            if ret == BANK_SYNTAX_ERROR:
                self._inf.add_msg("Sorry, I didn't quite catch that one.")
            elif ret == BANK_DEPOSIT_COPPER:
                self._inf.add_msg("You don't have that many copper coins.")
            elif ret == BANK_DEPOSIT_SILVER:
                self._inf.add_msg("You don't have that many silver coins.")
            elif ret == BANK_DEPOSIT_GOLD:
                self._inf.add_msg("You don't have that many gold coins.")
            elif ret == BANK_DEPOSIT_JADE:
                self._inf.add_msg("You don't have that many jade coins.")
            elif ret == BANK_DEPOSIT_MITHRIL:
                self._inf.add_msg("You don't have that many mithril coins.")
            elif ret == BANK_DEPOSIT_AMBER:
                self._inf.add_msg("You don't have that many amber coins.")
            elif ret == BANK_SUCCESS:
                if value:
                    self._inf.add_msg("You deposit {}.".format(CostString(value)))
                    self._inf.add_msg("Your new balance is {}.".format(CostString(self._activator.Controller().BankBalance())))
                else:
                    self._inf.add_msg("You don't have any money on hand.")

            self._inf.add_msg("Is there anything else that I can help you with?")
            self._service_links()
            return True

        # Make a withdrawal.
        elif msg == "dowithdraw":
            self._inf.add_msg("How much money would you like to withdraw from your account?")
            self._inf.add_msg("For example, type [green]1 mithril[/green], or [green]2 gold, 50 silver[/green], or [green]40 copper[/green], or [green]all[/green].")
            self._inf.set_text_input(prepend = "withdraw ")
            return True

        # Actually perform the withdrawal.
        elif msg.startswith("withdraw "):
            (ret, value) = self._activator.Controller().BankWithdraw(msg[9:])

            if ret == BANK_SYNTAX_ERROR:
                self._inf.add_msg("Sorry, I didn't quite catch that one.")
            elif ret == BANK_WITHDRAW_HIGH:
                self._inf.add_msg("You can't withdraw that much money at once.")
            elif ret == BANK_WITHDRAW_MISSING:
                self._inf.add_msg("You don't have that much money.")
            elif ret == BANK_WITHDRAW_OVERWEIGHT:
                self._inf.add_msg("You can't carry that much money.")
            elif ret == BANK_SUCCESS:
                balance = self._activator.Controller().BankBalance()

                if balance == 0:
                    self._inf.add_msg("You removed your entire balance of {}.".format(CostString(value)))
                else:
                    self._inf.add_msg("You withdraw {}.".format(CostString(value)))
                    self._inf.add_msg("Your new balance is {}.".format(CostString(balance)))

            self._inf.add_msg("Is there anything else that I can help you with?")
            self._service_links()
            return True

        # Check bank account balance.
        elif msg == "balance":
            balance = self._activator.Controller().BankBalance()

            if balance == 0:
                self._inf.add_msg("You have no money stored in your bank account.")
                self._inf.add_msg("Would you like to deposit all your money on hand?")
                self._inf.add_link("Sure.", dest = "deposit all")
            else:
                self._inf.add_msg("Your balance is {}.".format(CostString(balance)))
                self._inf.add_msg("Is there anything else that I can help you with?")
                self._service_links()

            return True

        # Explain about the bank system.
        elif msg == "explain":
            self._inf.add_msg("Certainly! It's my pleasure.")
            self._inf.add_msg("You can store store your money in any bank of the land. This can be very useful for an adventurer such as yourself, as the weight of the money will not encumber you.")
            self._inf.add_msg("Stored money can be withdrawn from your account at any time, from any bank.")
            self._inf.add_msg("You can also pay for services or items from merchants, priests and so on directly from your bank account, if you don't have enough money on hand.")
            self._inf.add_msg("Is there anything else that I can help you with?")
            self._service_links()
            return True

        return False

## @file
## API for handling taverns.

from Seller import Seller

class Bartender(Seller):
    max_goods = 100

    def subdialog_buyitem(self):
        self.add_msg("Ah, excellent choice!")
        self.subdialog_buy()
        self.add_msg("How many do you want to purchase?")
        self.subdialog_stockitem()

    def subdialog_boughtitem(self):
        self.add_msg("Here you go!")
        self.subdialog_bought()
        self.add_msg("Pleasure doing business with you!")

    def dialog_hello(self):
        self.add_msg("Welcome to my tavern, dear customer! I am {npc.name}, the bartender.")
        self.add_msg("I can offer you the following provisions.")
        self.subdialog_stock()

## @file
## Implements the House Agency Manager.

from Interface import Interface
from House import House

inf = Interface(activator, me)
house = House(activator, GetOptions())

def main():
    if msg == "hello":
        inf.add_msg("Welcome to our house agency. We can sell you a luxury house anyone would be jealous of! Do you want to buy one or do you need more information first?")
        inf.add_link("Tell me more.", dest = "tellmore")

        if not house.has_house():
            inf.add_link("I'd like to buy a Luxury House.", dest = "buy1")

    elif msg == "tellmore":
        inf.add_msg("Houses are like apartments, with the only difference that they are much bigger, so they can store more items, and they are buildable.")
        inf.add_msg("Also to enter your house you will need to pay the daily fees.")
        inf.add_link("Buildable?", dest = "buildable")
        inf.add_link("Tell me about the fees.", dest = "fees")

    elif msg == "buildable":
        inf.add_msg("Buildable areas of your house allow you to customize the house to your liking with things like chests, altars, walls, and so on. Of course, this all costs a price, but construction and materials are available at the secret island called <green>Everlink</green> which can be accessed from your house.")

    elif msg == "fees":
        inf.add_msg("The {} requires {} as daily fee to operate, and comes with {} days of prepaid fees. Fees are paid automatically whenever you enter the house (either from hand or bank acount). If you don't have enough money to pay, you won't be able to enter the house, but will be able to leave it and enter the <green>Everlink</green> area.".format(house.get(house.name), CostString(house.get(house.fee)), house.get(house.fees_prepaid)))
        inf.add_msg("Note that if you don't pay, for, say, 20 days, you won't have to pay for all those days that you did not use your house.")

    elif not house.has_house():
        if msg == "buy1":
            inf.add_msg("Certainly! The {} will cost you {}.".format(house.get(house.name), CostString(house.get(house.cost))))
            inf.add_msg("Are you sure that you want to buy this luxury house and that you understand the fees system?")
            inf.add_link("I'm sure.", dest = "buy2")
            inf.add_link("Tell me about the fees system.", dest = "fees")

        elif msg == "buy2":
            if activator.PayAmount(house.get(house.cost)):
                inf.add_msg("You pay {}.".format(CostString(house.get(house.cost))), COLOR_YELLOW)
                inf.add_msg("Thank you for your business! We hope you enjoy your brand new house.")
                house.add_house()
            else:
                inf.add_msg("You do not have enough money.")

main()
inf.finish()

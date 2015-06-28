## @file
## Script for the apartment seller in Brynknot.

from Atrinik import *
from Apartments import apartments_info
from Interface import Interface

inf = Interface(activator, me)


def main():
    apartment = apartments_info[GetOptions()]
    pinfo = activator.FindObject(archname = "player_info", name = apartment["tag"])

    if msg == "hello":
        from Language import l2s

        l = sorted(apartment["apartments"], key = lambda name: apartment["apartments"][name]["price"])
        inf.add_msg("Welcome to the {} apartment house.".format(me.map.region.longname))
        inf.add_msg("I can sell you an apartment, which is kind of a pocket dimension. Pocket dimensions are magical mini-dimensions in the Outer Plane. They are very safe and no thief will ever be able to enter.")
        inf.add_msg("I can offer you {} apartments.".format(l2s(l)))

        for name in l:
            inf.add_link("Tell me more about {} apartment.".format(name), dest = "buy1 " + name)

    elif msg.startswith("buy1 "):
        name = msg[5:]

        if not name in apartment["apartments"]:
            return

        inf.add_msg("[title]" + name.capitalize() + " Apartment[/title]")
        inf.add_msg(apartment["apartments"][name]["info"])

        if pinfo:
            inf.add_msg("Since you already own an apartment in this region, your current apartment would be replaced with this one, should you choose to purchase it. All your items would be safely transferred to the new apartment in that case.")

        inf.add_msg("The {} apartment will cost you {}. Is that okay?".format(name, CostString(apartment["apartments"][name]["price"])))
        inf.add_link("Yes.", dest = "buy2 " + name)
        inf.add_link("No.", dest = "hello")

    elif msg.startswith("buy2 "):
        name = msg[5:]

        if not name in apartment["apartments"]:
            return

        inf.add_msg("[title]" + name.capitalize() + " Apartment[/title]")

        if pinfo and pinfo.slaying == name:
            inf.add_msg("But it seems you already own the {} apartment here!".format(name))
            return

        if activator.PayAmount(apartment["apartments"][name]["price"]):
            inf.add_msg("You pay {}.".format(CostString(apartment["apartments"][name]["price"])), COLOR_YELLOW)
            inf.add_msg("Congratulations! You're now the proud owner of the {} apartment in this region.".format(name))

            if not pinfo:
                pinfo = activator.CreateObject("player_info")
                pinfo.name = apartment["tag"]
            else:
                activator.Controller().SwapApartments(apartment["apartments"][pinfo.slaying]["path"], apartment["apartments"][name]["path"], apartment["apartments"][name]["x"], apartment["apartments"][name]["y"])
                inf.add_msg("The items from your old apartment have been transferred successfully.")

            inf.add_msg("You can use the teleporter over there to enter your new apartment.")
            pinfo.slaying = name
        else:
            inf.add_msg("Sorry, you don't have enough money.")

main()
inf.send()

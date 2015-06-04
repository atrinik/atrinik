from Atrinik import *
from Interface import Interface

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()
inf = Interface(activator, me)

def main():
    if msg == "hello":
        import random

        replies = [
            "Get away from my *hic* beer *hic*!",
            "I can see you! *hic* Don' make me come af'er you! *hic* ... Can ye find my sword? I think I losst it!",
            "Bartender, 'nother beer here!",
            "[yellow]The " + me.name + " breaks into a song...[/yellow]\n\nI am a guard aaaand I am havin' a beer. Drinky drinky guard, drinky drinky guard.",
            "*HIC*",
            "I'm *hic!* watchin' you! I bets ye think I'm *hic* off duty but ye be wrong! I'm here guardin' my beer an' no-one iss gonna *hic!* take it from me!",
            "It's not fair! *hic!* I gets paid low wages! But I *hic* are super guard! We don' needs so many *hic* guards aroun' here! I can handles this whole *hic* here place meself! Noth'n can *hic* beat me! Hahahaha*hic!*!",
            "Ahhh! Stop that... shouting! It's making... people... me... nervous!",
        ]

        inf.add_msg(random.choice(replies))

main()
inf.finish()

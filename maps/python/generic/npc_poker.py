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
            "He's cheating! Waaaah!",
            "Don't distract me, I'm trying to win for once here!",
            "Won again! Who's your daddy, and what is his name?!",
            "Hey! Stop looking at my cards you rat!",
            "That's it, keep him distracted!",
            "Would you mind telling me what cards the other guy has? Teeheehee!",
        ]

        inf.add_msg(random.choice(replies))

main()
inf.finish()

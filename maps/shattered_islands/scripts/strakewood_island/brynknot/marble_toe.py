## @file
## Script for NPC called Marble Toe in Brynknot tavern.

from Interface import Interface
import random

inf = Interface(activator, me)

def main():
    if msg == "hello":
        replies = [
            "My apologies, your utterance does not make sense to me.",
            "I wish you actually know what you are doing or what you are saying.",
            "I will listen to you as long as it's logical, but I doubt I will ever need to.",
        ]

        inf.add_msg("Hello there, I'm {}, the president of the Society of Non-Aligned Brothers based in Thraal but at the moment I'm travelling to recruit more followers to my society.".format(me.name))
        inf.add_msg(random.choice(replies))

main()
inf.finish()

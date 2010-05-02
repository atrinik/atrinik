## @file
## Script for NPC called Marble Toe in Brynknot tavern.

from Atrinik import *
from random import choice

activator = WhoIsActivator()
me = WhoAmI()

replies = [
	"My apologies, your utterance does not make sense to me.",
	"I wish you actually know what you are doing or what you are saying.",
	"I will listen to you as long as it's logical, but I doubt I will ever need to.",
]

me.SayTo(activator, "\nHello there, I'm {0}, the president of the Society of Non-Aligned Brothers based in Thraal but at the moment I'm travelling to recruit more followers to my society.\n{1}".format(me.name, choice(replies)))

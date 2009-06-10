import Atrinik
import string

activator=Atrinik.WhoIsActivator()
whoami=Atrinik.WhoAmI()

msg = Atrinik.WhatIsMessage().strip().lower()
text = string.split(msg)

if text[0] == "healing":
	spell = Atrinik.GetSpellNr("minor healing")
	if spell == -1:
		whoami.SayTo(activator,"Unknown spell." )
	else:
		if activator.DoKnowSpell(spell) == 1:
			whoami.SayTo(activator,"You already know this prayer..." )
		else:	
			activator.AcquireSpell(spell, Atrinik.LEARN )

elif msg == 'food':
	activator.food = 500
	whoami.SayTo(activator,'\nYour stomach is filled again.')
	
elif msg == 'cast':
	whoami.SayTo(activator,
        "\nWell, to cast a prayer you need a deity.\n"
        "You should be a follower of the Tabernacle by now.\n"
        "(That should be written under your character name.)\n"
        "You can cast a spell or prayer in 2 ways:\n"
        "You can type /cast <spellname> in the console.\n"
        "In our case /cast minor healing.\n"
        "Or you can select the spell menu with F9.\n"
        "Go to the entry minor healing and press return over it.\n"
        "Then you can use it in the range menu like throwing.");

elif msg == 'done':
	whoami.SayTo(activator,
        "\nVery good. Now listen:"
        "\nI will teach you the prayer MINOR HEALING if you say\n"
        "^healing^ to me.\n"
        "But first you should ask me about how to ^cast^.\n"
        "I will tell you then the way you can cast a prayer or spell.");

elif msg == 'hello' or msg == 'hi' or msg == 'hey':
	whoami.SayTo(activator,
        "\nWelcome to the altar of the Tabernacle.\n"
        "To access the powers of the Tabernacle,\n"
        "You have to apply that altar next to me.\n"
        "Step over it and apply it.\n"
        "Then come back to me and say ^done^ to me."); 

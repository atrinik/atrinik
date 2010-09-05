## @file
## Implements Gashir, the owner of the Asterian Arms Tavern.

from Atrinik import *
from QuestManager import QuestManager
import string

activator = WhoIsActivator()
me = WhoAmI()

quest_arch_name = "barrel2.101"
quest_item_name = "Shipment of Charob Beer"

## Info about the quest.
quest = {
	"quest_name": "Shipment of Charob Beer",
	"type": QUEST_TYPE_SPECIAL,
	"message": "Deliver Charob Beer to the Asterian Arms Tavern.",
}

# Charob Beer Quest
qm = QuestManager(activator, quest)
qItem = activator.CheckInventory(2, quest_arch_name, quest_item_name)

NOQUEST = 0; QUESTCOMPLETE = 1; QUESTONLY = 2; QUESTANDITEM = 3; QUESTLOSTITEM = 4

if not qm.started():
    quest_beer = NOQUEST        # No Quest
elif qm.completed():
    quest_beer = QUESTCOMPLETE	# Quest complete
elif qItem:
    quest_beer = QUESTANDITEM	# Quest and Item 
elif qm.quest_object.last_sp == 0:
    quest_beer = QUESTLOSTITEM  # Quest item was lost somehow?
else:
    quest_beer = QUESTONLY	# Quest only

# 0=disable, 1=enable
DEBUGON = 1

if DEBUGON == 1:
    stQStatus = ('NOQUEST','QUESTCOMPLETE','QUESTONLY','QUESTANDITEM', 'QUESTLOSTITEM')
    activator.Write("QUEST_DEBUGGING ON:\nCharobBeer:"+stQStatus[quest_beer])


msg = WhatIsMessage().strip().lower()
text = string.split(msg)

def main():
    if quest_beer == QUESTANDITEM:
        me.Communicate("/smile")
        me.SayTo(activator," Finally, I get my shipment of Charob Beer!")
        activator.Write("You give the %s to %s." % (qItem.slaying,me.name.split()[0]), COLOR_WHITE)
        # flag that player actually DID the quest and didn't lose the item somehow...
        qm.quest_object.last_sp = 1
        qItem.Remove()

    elif quest_beer == QUESTLOSTITEM:
        me.SayTo(activator, "I was told I would be getting a shipment.  Where is it, then?!")
        me.SayTo(activator, "Fortunately, someone else delivered some while you were off losing my beer.")
        qm.complete()

    elif msg == "this ale stinks":
	    me.Say("If you don't like it, find another tavern!")

    elif msg == 'booze':
        if activator.PayAmount(5) == 1:
            me.SayTo(activator,"\nHere you go! Enjoy!")
            activator.CreateObjectInside("booze_generic", 1,1,1)
            activator.Write( "You pay the money.", 0)
            # give item booze
        else:
            me.SayTo(activator,"\nSorry, you do not have enough money.")

    elif msg == 'strong booze':
        if activator.PayAmount(10) == 1:
            me.SayTo(activator,"\nHere you go! But be careful, it is really strong!")
            activator.CreateObjectInside("poison_food", 1,1,1)
            activator.Write( "You pay the money.", 0)
        else:
            me.SayTo(activator,"\nSorry, you do not have enough money.")
    
    elif msg == 'charob beer':
        if not qm.started():
            me.SayTo(activator,"\nAh, sorry.  We are fresh out!  Maybe you could check down at the brewery to see what is holding my shipment up?")
        elif activator.PayAmount(8) == 1:
            me.SayTo(activator,"\nHere you go! It is quite good quality!")
            object = activator.CreateObjectInside("booze_generic",0,1)
            object.name = "Charob Beer"
    	    object.food = 600
            activator.Write( "You pay the money.", 0)
        else:
            me.SayTo(activator,"\nSorry, you do not have enough money.")

    elif msg == 'water':
        if activator.PayAmount(2) == 1:
            me.SayTo(activator,"\nThirsty? Nothing like fresh water!")
            activator.CreateObjectInside("drink_generic", 1,1,1)
            activator.Write( "You pay the money.", 0)
        else:
            me.SayTo(activator,"\nSorry, you do not have enough money.")

    elif msg == 'food':
        if activator.PayAmount(10) == 1:
            me.SayTo(activator,"\nHere you go! It's really tasty, I tell you.")
            activator.CreateObjectInside("food_generic", 1,1,1)
            activator.Write( "You pay the money.", 0)
        else:
            me.SayTo(activator,"\nSorry, you do not have enough money.")

#    elif msg == 'key' or msg == 'room key' or msg == 'basement room' or msg == 'basement rooms' or msg == 'locked door':
#        me.SayTo(activator,"\nBy now, rooms in basement are not yet ready to rent. Try tommorow."
#                 +" Renting room for just 15 copper will allow you to rest once."
#                 +" Then you will need to buy key again.")
    elif msg == 'complain':
        me.SayTo(activator,'\nI told you it is really strong! What did you expect? Elvish wine?')

    elif msg == 'hello' or msg == 'hi' or msg == 'hey' or msg == 'yo':
        me.SayTo(activator,"\nWelcome to Asterian Arms Tavern\n"
                 +" I'm Gashir, the bartender of this tavern. Here is the"
    		     +" place if you want to eat or drink the best booze hereabout!"
                 +" I can sell you ^booze^ for just 5 copper,"
                 +" really ^strong booze^ for 10 copper or ^food^ for 10 copper."
                 +" We have also some ^water^, for 2 copper, and finally our local "
                 +" special, ^Charob Beer^, for 8 copper.  Also, please, do not "
                 +" ^complain^ about quality of strong booze.\n") 
    else:
        pass
        #activator.Write("%s listen to you without answer." % whoami.name, 0)
        #test

main()
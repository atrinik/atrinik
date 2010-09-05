## @file
## Implements Steve Bruck, the warehouse guy for Charob Beer.

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
        me.Communicate("/frown")
        me.SayTo(activator," Are you planning to take that delivery to Gashir anytime soon? The boss is really gonna get mad at me if its not delivered soon.")
 
    elif quest_beer == QUESTLOSTITEM:
        me.SayTo(activator, "I just gave it to you!  How could you lose the shipment?!")        
        qm.complete(None)

    elif quest_beer == QUESTONLY:
        me.Communicate("/me smiles at you appreciatively.")
        me.SayTo(activator,"Thank you! I hope he wasn't too upset about the late delivery!")
        me.SayTo(activator,"Here is your payment...")
        activator.CreateObjectInside("silvercoin",0,5)  # this is the reward
        activator.Write("You received 5 silver coins.",COLOR_WHITE)
        qm.complete() 

    elif msg == 'hello' or msg == 'hi' or msg == 'hey':
        me.SayTo(activator,"\nWelcome to Charob Beer's Shipping Department.\nI am as usual overworked and underpaid.")	
        if quest_beer == NOQUEST:
            me.SayTo(activator,"I also have this ^shipment^ of booze gathering dust here.")
        elif quest_beer == QUESTANDITEM:
            me.SayTo(activator,"Hurry up and get that beer to Gashir before I lose my job!")
        elif quest_beer == QUESTCOMPLETE:
            me.SayTo(activator,"Thanks for the help with that ^shipment^, I should have another one soon.")

    elif msg == 'shipment':
        if quest_beer == NOQUEST:
            me.SayTo(activator,"Great! My body is terribly sore, and I've got a lot of shipments to sort. Please deliver this shipment to Gashir at the Asterian Arms Tavern.")
            # Create the quest object (arche type, name that is displayed in inventory) #
            qm.start(None)
            qm.quest_object.last_sp = 0
            qm.quest_object.last_grace = 1
            # Create the crop bag in the players inventory #
            the_shipment = activator.CreateObjectInside(quest_arch_name,0,1)
            the_shipment.name = quest_item_name
#            the_shipment.quest_item = 1
            the_shipment.f_no_drop  = 0 # For purposes of debugging, keep this at 0.. change to 1 otherwise
            the_shipment.f_no_apply = 1
            activator.Write("You take the Shipment of Charob Beer",COLOR_WHITE)
            activator.Write("*Quest Added*",COLOR_WHITE)
        elif quest_beer == QUESTCOMPLETE:
            me.SayTo(activator,"Thanks for taking that shipment to Gashir. I'll have another one ready soon, so be ready.")
            #To be continued?

main()
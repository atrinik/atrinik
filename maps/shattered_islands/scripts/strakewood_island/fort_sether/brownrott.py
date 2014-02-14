## @file
## Handles Brownrott the kobold, and his involvement in the 'Fort Sether Illness'
## quest.

from QuestManager import QuestManagerMulti
from Quests import FortSetherIllness as quest
from Interface import Interface

qm = QuestManagerMulti(activator, quest)
inf = Interface(activator, me)

def main():
    # Agreed to investigate the illness.
    if not qm.completed("figure"):
        if msg == "hello":
            inf.add_msg("Well, hello there! I am {} the kobold. Don't you think my garden is just beautiful?".format(me.name))
            inf.add_link("Your garden?", dest = "garden")

        elif msg == "garden":
            inf.add_msg("Why yes! Just look around you! I use a specially crafted potion that keeps my garden looking nice. Do you want to see?")
            inf.add_link("Why not...", dest = "whynot")

        elif msg == "whynot":
            inf.add_msg("Here it is! Doesn't it smell wonderful?")
            inf.add_msg("You start feeling nauseous as soon as the potion is opened...", COLOR_YELLOW)
            inf.add_msg("Oh right, sorry! Let me close it again... I forgot it has ingredients that make most creatures sick...")

            # Now we know the cause of the illness, onto the next part we go.
            if qm.started("figure"):
                inf.add_msg("As the potion closes, you start feeling better again. Perhaps you should report your findings to Gwenty...", COLOR_YELLOW)
                qm.start("ask advice")
                qm.complete("figure")
            else:
                inf.add_msg("As the potion closes, you start feeling better again.", COLOR_YELLOW)
    # Reported to Gwenty about the kobold.
    elif qm.need_complete("deliver potion") and activator.FindObject(mode = INVENTORY_CONTAINERS, name = "Gwenty's Potion"):
        # Accept Kobold's mini-quest
        if not qm.started("get hearts"):
            if msg == "hello":
                inf.add_msg("Well, hello there again! My garden is just bea--")
                inf.add_msg("You interrupt Brownrott and explain to him about the illness in Fort Sether and his potion...", COLOR_YELLOW)
                inf.add_msg("Are you sure? Hmm... I don't know... I don't really trust anyone with my potion except myself... But perhaps... If you bring me something tasty, I might change my mind...")
                inf.add_link("What do you want?", dest = "want")

            elif msg == "want":
                inf.add_msg("Spider hearts, of course, I want spider hearts - they are very tasty, but the spiders are dangerous.")
                inf.add_msg("Bring me 10 sword spider hearts, and I'll mix your potion with mine. You can find those spiders around in this cave. I usually stay far away from them, but their hearts sure are delicious...")
                qm.start("get hearts")
        # Finished his mini-quest yet?
        elif qm.finished("get hearts"):
            if msg == "hello":
                inf.add_msg("Mmm! Delicious sword spider hearts! I like you! Mmm! Alright, let's mix the potions then!")
                inf.add_msg("You hand the 10 sword spider hearts and the potion to Brownrott.", COLOR_YELLOW)
                activator.FindObject(mode = INVENTORY_CONTAINERS, name = "Gwenty's Potion").Remove()
                qm.start("reward")
                qm.complete("get hearts")
                qm.complete("deliver potion")
                inf.add_msg("There! All done. Thank you again for the sword spider hearts! Mmm!")
                inf.add_msg("You should report the good news to Gwenty.", COLOR_YELLOW)
        # Not yet.
        else:
            if msg == "hello":
                inf.add_msg("Bring me 10 sword spider hearts, and I'll mix your potion with mine. You can find those spiders around in this cave. I usually stay far away from them, but their hearts sure are delicious...")
    # Completed the quest.
    else:
        if msg == "hello":
            inf.add_msg("Well, hello there again! My garden is just beautiful, isn't it?")
            inf.add_link("Right...", action = "close")

main()
inf.finish()

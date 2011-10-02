from Interface import Interface
from QuestManager import QuestManagerMulti
from Quests import LlwyfenPortal as quest

inf = Interface(activator, me)
qm = QuestManagerMulti(activator, quest)

def main():
	if not qm.started_part(3):
		if msg == "hello":
			inf.add_msg("Greetings citizen. I'm the {}, the captain of the Brynknot Guards, and our duty as guards is to protect the citizens of Brynknot.".format(me.name))
			inf.add_msg("Be careful if you're going out of the city.")

	elif not qm.completed_part(3):
		if msg == "hello":
			inf.add_msg("So you're the adventurer Maplevale sent word of. The runner said that there are enemies beneath Brynknot, somewhere in the sewers, is that correct?")
			inf.add_link("Yes, I found them through a portal in Underground City.", dest = "yesuc")

		elif msg == "yesuc":
			inf.add_msg("Hmm. According to the information the mayor sent me, your description of the place beyond the portal matches that of a part of the Brynknot sewer system that has been sealed off. It leads to a maze-like part of the sewers, dug out by monsters, and not by humans. But I don't know who is responsible for all of this.")
			inf.add_msg("How can I get to that part of the sewers?", dest = "getto")

		elif msg == "getto":
			inf.add_msg("Here, you'll need this key. The gate is sealed shut, but this key should still be able to open it.")
			inf.add_objects(me.FindObject(archname = "key_brynknot_sewer_maze"))
			inf.add_msg("Go through the portal once again, open the gate, and please find whoever is responsible for all this.")
			qm.start(4)
			qm.complete(3, sound = None)

	elif qm.started_part(4):
		if msg == "hello":
			inf.add_msg("Have you found whoever is responsible for the attack?")

			if qm.finished(4):
				inf.add_link("Yes, the beholder Nyhelobo has been taken care of.", dest = "takencare")
			else:
				inf.add_link("Working on it...", action = "close")

		elif qm.finished(4):
			if msg == "takencare":
				inf.add_msg("That's excellent news, {}.".format(activator.name))
				inf.add_msg("You're not one of my guards, but you still deserve to be paid for your service. Who knows, if not for you, Brynknot might have been overrun by demons or worse... So, please accept this payment.")
				inf.add_objects(me.FindObject(archname = "goldcoin"))
				qm.complete(4)

	else:
		if msg == "hello":
			inf.add_msg("Welcome, welcome. We won't forget what you have done for Brynknot, {}.".format(activator.name))

main()
inf.finish()

from Interface import Interface
from QuestManager import QuestManagerMulti
from Quests import LostMemories as quest
from Packet import Notification

qm = QuestManagerMulti(activator, quest)
inf = Interface(activator, me)

def main():
	if not qm.started_part(1):
		if msg == "hello":
			inf.add_msg("There you are! I didn't wake you up when we arrived in Incuna, because it seemed like you needed a good rest. You certainly did - you've been sleeping for quite a while now.")
			inf.add_msg("At any rate, we did it - we managed to reach Incuna. We should resupply and head to Strakewood at last - but don't worry, I'll handle that and anything else we may need.")
			inf.add_msg("However... there's still the question of what to do about your memory loss. Say, has it improved?")
			inf.add_link("I feel I'm starting to remember...", dest = "remember")
			inf.add_link("Not really...", dest = "remember")

		elif msg == "remember":
			inf.add_msg("Well, at any rate, I think you should visit the local church priest. Manard is his name, if I remember correctly. You should be able to find directions to him easily, perhaps by asking around the local townsfolk. A map would come in handy, hmm... I know, go visit Gulliver in the dock house. He's an old friend of mine, he might have a spare map for you.")
			inf.add_msg("Ah, one last thing! Here, take the rest of our mushroom supplies - I'll get us something better. No offense, but I feel I wouldn't be able to stomach any more of those things...")
			sack = me.FindObject(archname = "sack").Clone()
			inf.add_msg_icon(sack.face[0], "sack with mushroom supplies")
			sack.InsertInto(activator)
			inf.add_link("I'll do that.", action = "close")
			Notification(activator.Controller(), "Tutorial Available: Containers", "/help basics_containers", "?HELP", 90000)
			qm.start(1)

	elif qm.completed():
		if msg == "hello":
			inf.add_msg("Ah, good! You seem much more confident now - almost the same person I have seen in you when you first came to me and hired me to transport you to Strakewood.")
			inf.add_msg("Very well then. Are you packed up and ready to go?")
			inf.add_link("I'm ready.", dest = "ready")
			inf.add_link("Not just yet...", action = "close")

		elif msg == "ready":
			inf.close_dialog()

	else:
		if msg == "hello":
			inf.add_msg("Hmmm... You don't seem ready yet, {}. I am sorry, but unless you regain at least part of your memory, there's no reason for us to sail to Strakewood...".format(activator.name))

main()
inf.finish()

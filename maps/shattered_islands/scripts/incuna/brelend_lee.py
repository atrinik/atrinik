from Interface import Interface
from QuestManager import QuestManagerMulti
from Quests import LostMemories as quest
from Packet import Notification
import Temple

qm = QuestManagerMulti(activator, quest)
inf = Interface(activator, me)

def main():
	if qm.started_part(1) and not qm.started_part(2):
		if msg == "hello":
			inf.add_link("I need help recovering my memories.", dest = "helprecover")

		elif msg == "helprecover":
			inf.add_msg("Recovering your memories, you say? Hmm. That is quite difficult business indeed, and often near impossible.")
			return

	temple = Temple.TempleTabernacle(activator, me, inf)
	temple.handle_chat(msg)

main()
inf.finish()

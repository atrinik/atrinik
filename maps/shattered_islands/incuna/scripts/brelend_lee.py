from Interface import Interface
from QuestManager import QuestManagerMulti
from Quests import LostMemories as quest
from Packet import Notification
import Temple

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()
qm = QuestManagerMulti(activator, quest)
inf = Interface(activator, me)

class TempleCustom(Temple.TempleTabernacle):
	pass

def main():
	if qm.started_part(1) and not qm.started_part(2):
		def temple_post_init(self):
			self.services = []

		TempleCustom.post_init = temple_post_init
		Temple.handle_temple(TempleCustom, me, activator, msg)

main()
inf.finish()

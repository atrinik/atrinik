## @file
## Quest to get gate key on Tutorial Island.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

quest = {
	"quest_name": "Tutorial Wolf Cubs",
	"type": QUEST_TYPE_KILL,
	"kills": 5,
	"message": "Kill 5 wolf cubs on the Tutorial Island, then return to Tommy to get the gate key.",
}

qm = QuestManager(activator, quest)

def main():
	if msg == "hello" or msg == "hi" or msg == "hey":
		if not qm.started():
			me.SayTo(activator, "\nHowdy {}, I am {}.\nYou are almost done! This is the last part of Tutorial Island, and I have a little quest for you to ensure you are ready for the real world.\n\nWill you <a>accept</a> this quest?".format(activator.name, me.name))
		elif qm.completed():
			me.SayTo(activator, "\nGo on through the gate and enter the portal to end the tutorial and enter the real world.")
		elif qm.finished():
			key = me.FindObject(archname = "key_brown")
			key.Clone().InsertInto(activator)
			me.SayTo(activator, "\nExcellent work! As I promised, here's the key...\n<yellow>{} hands you {}.</yellow>\nGo on through the gate and enter the portal to end the tutorial and enter the real world.".format(me.name, key.name))

			for skill in ("slash weapons", "impact weapons", "cleave weapons", "pierce weapons"):
				skill_id = GetSkillNr(skill)

				if activator.Controller().DoKnowSkill(skill_id):
					activator.Controller().AddExp(skill_id, 1500, True)
					break

			qm.complete()
		else:
			to_kill = qm.num_to_kill()
			me.SayTo(activator, "\nYou still need to kill {} wolf cub{}.".format(to_kill, "s" if to_kill > 1 else ""))

	elif not qm.started():
		if msg == "accept":
			me.SayTo(activator, "\nAlright then. I need you to kill {} of the nearby wolf cubs. Once you are done, return to me and I will give you a key which will allow you to leave through the gate.".format(quest["kills"]))
			qm.start()

main()

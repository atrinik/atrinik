## @file
## Consecrate prayer quest.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()

quest = {
	"quest_name": "Vielumin Ruins Altar",
	"type": QUEST_TYPE_SPECIAL,
	"message": "Turium the elven priest in Burnt Forest has instructed you to go to Vielumin Ruins - which are in the northeast part of the Burnt Forest - and find Dalosha's Altar, then use the scroll he has given you to consecrate the altar to the elven goddess Tylowyn, thus weakening the drows.",
}

qm = QuestManager(activator, quest)

def npc_turium():
	if msg == "hi" or msg == "hey" or msg == "hello":
		if not qm.started():
			me.SayTo(activator, "\nWelcome {}. I am sorry, but we are quite busy protecting this area from the angry trees and <a>drows</a>...".format(activator.name))
		elif qm.completed():
			me.SayTo(activator, "\nI am still pondering a way to weaken the drows in the <a>ruins</a> to the northeast... Or are you here to learn about the <a>consecrate</a> prayer?")
		else:
			# Check if the player used the scroll already.
			if activator.FindObject(INVENTORY_CONTAINERS, name = me.FindObject(archname = "scroll_generic").name):
				me.SayTo(activator, "\nYou still have my scroll... Please go to the <a>ruins</a>, find Dalosha's altar, and use the scroll to consecrate the altar to the elven goddess Tylowyn. I am not sure how strong those creatures in the ruins are... But if the task is beyond you... You should go and gain some more experience, then try again later.")
			else:
				# Some small reward.
				reward = CreateObject("silvercoin")
				reward.nrof = 60
				me.SayTo(activator, "\nYou have done it!\n<yellow>You tell {} what happened.</yellow>\n... Oh. ... That is indeed very troublesome news, {}. It seems the drows are much more powerful than we thought... Hmm...\nWell, it seems you at least learned the <a>consecrate</a> prayer in the process. I also have something here for your trouble...\n<yellow>You receive {}.</yellow>\nThank you for trying, I will have to think of something else...".format(me.name, activator.name, reward.GetName()))
				reward.InsertInto(activator)
				qm.complete()

	elif msg == "ruins":
		me.SayTo(activator, "\nThe Vielumin Ruins are to the northeast. We do not dare approach the place, because not only drows live there, but also much more sinister creatures, from what we know.")

	# Explain the consecrate prayer if we have completed the quest.
	elif qm.completed():
		if msg == "consecrate":
			me.SayTo(activator, "\nThe consecrate prayer allows you to turn an altar into shrine of your religion, showing devotion to your god or goddess. However, if the altar has already been consecrated previously to another god, your attempt will fail if the priest that did the job was higher level in divine prayers than you are.")

	# Background information.
	elif not qm.started():
		if msg == "drows":
			me.SayTo(activator, "\nI said we were busy... But you seem quite curious. Alright then. I'll tell you...\nAll the fighting started when the living creatures around here were burned - and some were killed - by this huge <a>fire explosion</a>. Most of the creatures are mad with anger, and they attack even us, the elves.")
		elif msg == "fire explosion":
			me.SayTo(activator, "\nI bet the drows living in the <a>ruins</a> to the northeast are behind that explosion. They dislike us - the elves - and no doubt wanted to hurt us. It may even have something to do with their <a>twisted goddess</a>...")
		elif msg == "twisted goddess":
			me.SayTo(activator, "\nThe rebellious heretic, Dalosha... I have been pondering this, and came to the conclusion that we may weaken the drows in the <a>ruins</a> to the northeast by <a>consecrating</a> their altar to the elven goddess, Tylowyn.")
		elif msg == "consecrating":
			me.SayTo(activator, "\nConsecrating allows one to turn altars into shrines of their religion, and show devotion to their god or goddess. But because of the sinister creatures in the <a>ruins</a>, none of our elves dare approach the place.\n\n<a>Could I help?</a>")
		elif msg == "could i help?":
			if activator.Controller().GetSkill(Type.SKILL, GetSkillNr("divine prayers")).level < 30:
				me.SayTo(activator, "\nI appreciate the offer, but you lack enough divine prayers skill experience. You would need at least level 30 in the divine prayers skill...")
			else:
				scroll = me.FindObject(archname = "scroll_generic").Clone()
				me.SayTo(activator, "\nHmm... It seems you might be able to accomplish this task. Alright then, go to the <a>ruins</a>, find Dalosha's altar, and use this scroll to consecrate the altar to the elven goddess Tylowyn.\n<yellow>{} hands you {}.</yellow>\nPlease be extremely careful in there!".format(me.name, scroll.GetName()))
				scroll.InsertInto(activator)
				qm.start()

## Handles applying Turium's scroll of consecrate.
def scroll_apply():
	# Find the appropriate altar under the player, if any.
	for obj in activator.map.GetFirstObject(activator.x, activator.y):
		# Check if it is the altar we want.
		if obj.type == Type.HOLY_ALTAR and obj.ReadKey("vielumin_ruins_altar"):
			# Allow the scroll to run but fail (not powerful enough), but give the player the prayer.
			activator.Write("It seems Turium's theory was wrong -- the scroll failed to consecrate the altar... However, you managed to write down the process so you can use it later on other altars. Perhaps you should tell Turium what happened...", COLOR_YELLOW)
			activator.Controller().AcquireSpell(GetSpellNr("consecrate"))
			return

	activator.Write("You are not standing over Dalosha's altar in the Vielumin Ruins.", COLOR_WHITE)
	SetReturnValue(1)

# Decide what to do depending on the action.
if GetEventNumber() == EVENT_SAY:
	msg = WhatIsMessage().strip().lower()

	if me.name.find("Turium") != -1:
		npc_turium()
else:
	scroll_apply()

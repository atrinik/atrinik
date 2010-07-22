## @file
## Script for elf Maplevale in Greyton church and Talthor the Guard Captain
## in Brynknot.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()

msg = WhatIsMessage().strip().lower()

## Maplevale's quest.
quest_maplevale = {
	"quest_name": "Investigation for Maplevale",
	"type": QUEST_TYPE_KILL_ITEM,
	"arch_name": "note",
	"item_name": "Letter from Nyhelobo to oty captain",
	"message": "Investigate the strange portal in Underground City which requires an amulet of Llwyfen to enter, then return to Maplevale in Greyton.",
}

## Talthor's quest.
quest_talthor = {
	"quest_name": "Enemies beneath Brynknot",
	"type": QUEST_TYPE_KILL,
	"kills": 1,
	"message": "Go through the Brynknot Sewers Maze, which you can access by taking Llwyfen's portal in Underground City, and kill whoever is responsible for the planned attack on Brynknot, then return to Guard Captain Talthor in Brynknot.",
}

# Initialize quest managers.
qm_m = QuestManager(activator, quest_maplevale)
qm_t = QuestManager(activator, quest_talthor)

## Give amulet blessed by Llwyfen to the player.
## @return The amulet given.
def give_amulet():
	clone = me.CheckInventory(0, "key2", "Maplevale's amulet", "of Llwyfen").Clone()
	clone.InsertInside(activator)
	return clone

## Give a key for Brynknot Maze to the player.
def give_key():
	me.CheckInventory(0, "key2", "Brynknot Maze Key").Clone().InsertInside(activator)

## Handle Maplevale.
def npc_maplevale():
	# Is our god Tabernacle?
	is_tabernacle = activator.GetGod() == "Tabernacle"
	## Have we found the UC portal?
	found_portal = activator.Controller().quest_container.ReadKey("underground_city_lake_portal")

	if msg == "hi" or msg == "hey" or msg == "hello":
		me.SayTo(activator, "\nGreetings, {0}. I am {1}, a priest of the Elven god Llwyfen.".format(activator.name, me.name))

		if is_tabernacle:
			me.SayTo(activator, "I am here to learn the ^teachings^ of the god of Tabernacle.", 1)

		if found_portal:
			if not qm_m.started():
				me.SayTo(activator, "\n^I found Llwyfen's portal^", 1)
			elif qm_m.completed():
				if qm_t.completed():
					me.SayTo(activator, "\nThank you for saving Brynknot.", 1)
				else:
					me.SayTo(activator, "\nPlease go see Talthor the Brynknot Guard Captain immediately.", 1)
			elif not qm_m.finished():
				me.SayTo(activator, "\nHow's the investigation going? Still no results?\nWell, make haste then!", 1)
			else:
				me.SayTo(activator, "\nHmm! These notes are most troubling indeed.\nPlease go see Talthor the Brynknot Guard Captain immediately.", 1)
				qm_m.complete()

		return

	# Quest if we found the portal.
	if found_portal:
		if not qm_m.started():
			if msg == "i found llwyfen's portal":
				me.SayTo(activator, "\nHmm... So you're saying you found a portal that requires an amulet of Llwyfen to enter in the Underground City? That is the most unsettling news I have heard about that area recently...\nWould you please go investigate it?\n\n^Sure^")
				return

			if msg == "sure":
				me.SayTo(activator, "\nTake this amulet with you so you can pass through the portal.")
				activator.Write("You receive {0}.".format(give_amulet().GetName()), COLOR_GREEN)
				qm_m.start()
				return

	# Some information about gods if our god is Tabernacle.
	if is_tabernacle:
		if msg == "teachings":
			me.SayTo(activator, "\nYes, words of wisdom. But alas, so far I have not been able to hear any particular teaching from the priests of Tabernacle. There seems to be none who ever had a direct ^interaction^ with the god.")

		elif msg == "interaction":
			me.SayTo(activator, "\nYes, like seeing, listening, and talking to the god. Strange enough, none of the priest even knows the real name of the god of Tabernacle, and still call the god by the name of the altar! It seems your god has not ^revealed^ himself to anyone yet.")

		elif msg == "revealed":
			me.SayTo(activator, "\nIn our legend, the Tabernacle was crafted by Vashla, the Goddess of Vanity. Some elves believe that this goddess is the God of Tabernacle but I have ^confirmed^ that these two are totally different.")

		elif msg == "confirmed":
			me.SayTo(activator, "\nOh certainly! Vashla was the Goddess of Vanity. What do you expect? It was very difficult to satisfy her appetite for praise. That was one of the reasons the elves of Eromir refused to worship her. I know it because I once saw the book of liturgy.\n\nOh I shouldn't make fun of a goddess. Anyway, the point is, deities do not change and Vashla wouldn't have allowed her followers to worship her like you do. As I observe, you do not even have any other ritual than touching the altar, while fully enjoying his ^gifts^.")

		elif msg == "gifts":
			me.SayTo(activator, "\nResurrections, of course! A great and powerful god he must be.\n\nHmmm... it leads me to wonder. If the god of Tabernacle is not Vashla, who is he then? What is his purpose and why is he helping you? Maybe your gods are different from Elven gods? Oh don't worry, I was merely thinking out loud.")

## Handle Guard Captain Talthor
def npc_talthor():
	if msg == "hi" or msg == "hey" or msg == "hello":
		me.SayTo(activator, "\nHello fellow citizen. I'm the {0}, and our duty as guards is to protect the citizens of Brynknot.\nBe careful if you're going out of the city.".format(me.name))

		if not qm_t.started():
			if qm_m.completed():
				me.SayTo(activator, "\nSo the city's major sent you? Oh, you don't know! Maplevale is the Brynknot major. What's the matter?\n\n^There are enemies under Brynknot^", 1)
		elif qm_t.completed():
			me.SayTo(activator, "\nThank you for saving our city.", 1)
		elif qm_t.finished():
			me.SayTo(activator, "\nYou did it! Brynknot is safe now thanks to you.\nAs a reward, I'll teach you the secret knowledge of the firebolt spell.")

			skill = activator.GetSkill(TYPE_SKILL, GetSkillNr("wizardry spells"))
			spell = GetSpellNr("firebolt")

			if not skill:
				me.SayTo(activator, "\nHowever, you seem to lack the wizardry spells skill...", 1)
			elif skill.level < 70:
				me.SayTo(activator, "\nYour wizardry spells skill is too low. Come back at level 70 wizardry.", 1)
			else:
				me.SayTo(activator, "\nNow, let me teach you the knowledge of firebolt...", 1)
				activator.AcquireSpell(spell, LEARN)
				qm_t.complete()
		else:
			me.SayTo(activator, "\nGo and kill whoever is responsible for the planned attack as fast as you can.")

	# Have we completed Maplevale's quest?
	elif qm_m.completed():
		if not qm_t.started():
			if msg == "there are enemies under brynknot":
				me.SayTo(activator, "\nWhat?! Tell me more about this.\n...\nI see. The passage you have discovered through the portal is actually a part of the Brynknot sewers, but it's been sealed off. It leads to a maze-like part of the sewers, dug out by monsters, and not by humans. But I don't know who is responsible for all of this. Would you go and try to kill their boss, whoever it is?\n\n^Okay^")

			elif msg == "okay":
				me.SayTo(activator, "\nThe fate of our city rests upon you. Take this key, which will unlock the gate in the passage for you. Then kill whoever is responsible for the attack.")
				give_key()
				activator.Write("{0} hands you a key.".format(me.name), COLOR_GREEN)
				qm_t.start()

if me.name == "Maplevale":
	npc_maplevale()
else:
	npc_talthor()

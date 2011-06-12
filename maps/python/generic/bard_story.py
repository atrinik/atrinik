## @file
## Implements bards that tell stories, piece by piece, to all nearby
## players.

from Atrinik import *

activator = WhoIsActivator()
me = WhoAmI()
event = WhatIsEvent()

## All the possible stories.
stories = {
	"morliana": [
		("Hierro's Story", [
			"Hierro lived in his home,\nadmiring his gems and precious stones...",
			"Until one day, something caused a stir.\nIntruders were coming to take his treasure...",
			"Defend his dwelling, defend it fast.\nRemoving the torches, from their path...",
			"Darkness his friend, filled his house.\nHe could see the heat that they gave out...",
			"Picking them off, one by one.\nLike a thief, it had to be done...",
			"He drove them off, from his house.\nBut they would come back, for another bout...",
			"They brought a wizard, who set a trap.\nTo teleport Hierro all the way back...",
			"All the way back, to the bottom of the cave,\nHe would have to find his way...",
			"To the exit, not easy to find.\nHierro searched for a very long time...",
			"He sees the exit, it is so near.\nBut it is just as he feared...",
			"He could not leave, because of the spell.\nIt trapped him there, his home now his hell...",
			"Gone from his home, his gems and stones.\nWhatever happened to Hierro, no one knows."]
		),
		("About Hierro", [
			"Look and see, what I have found.\nCrystals and gems, in the walls and on the ground...",
			"In a cave, just south of the town.\nIs where to look, they are all around...",
			"Spread the word, there is more than enough.\nGetting these riches should not be tough...",
			"A party has formed, we are going to mine.\nThese riches will be ours in good time...",
			"As we got started, something took us by surprise.\nAll we could see were glowing eyes...",
			"It picked us off, one by one.\nThat is when we started to run...",
			"Away from that place, away from the cave.\nBefore we ended up, in a grave...",
			"As we left, we heard a bellow.\nStay out of my house, my name is Hierro...",
			"What was this creature that took us out.\nI think it's time that we found out...",
			"To the wizard, he will set a trap.\nTo get rid of the creature, that started the attack...",
			"The spell succeeded, Hierro has been teleported away.\nThe cave is ours, we can mine it today...",
			"To this day, we still do not know\nwhat kind of creature was Hierro."]
		),
		("History of the Founding of Morliana", [
			"They saw an object, when they looked up high.\nGetting brighter and brighter in the sky...",
			"They looked with wonder and started to fear.\nThe object looked to be getting near...",
			"They looked for cover, thinking this was their fate.\nNo time to run, it was too late...",
			"The object missed them, it went right by.\nIt crashed on an island that was near by...",
			"They gathered the people, to investigate.\nAlthough the task would be very great...",
			"The island was known, to be very cold.\nWith proper supplies and only the bold...",
			"Would be sent, on the day.\nThe party was off on their way...",
			"In due time, they found the place.\nA glowing Blue Crystal in front of their face...",
			"The closer they got, they could not believe.\nAll of the crystals and gems beneath their feet...",
			"The Great Blue Crystal, seemed to be the source.\nIt glowed with power, giving off warmth...",
			"They gathered the crystals, all they could take.\nAnd built a village with the money they made."]
		),
	],
}

## Handle the NPC.
def main():
	# Performing, ignore everything.
	if me.ReadKey("performing"):
		return

	msg = WhatIsMessage().strip().lower()

	# Show available plays.
	if msg == "hi" or msg == "hey" or msg == "hello":
		me.SayTo(activator, "\nGreetings {0}, I am {1}.\nIf you like, I can perform the following plays:\n\n{2}".format(activator.name, me.name, "\n".join(list(map(lambda l: "<a>" + l[0] + "</a>", stories[GetOptions()])))))
	# See if player chose one of the plays.
	else:
		l = list(filter(lambda l: l[0].lower() == msg, stories[GetOptions()]))

		if not l:
			return

		l = l[0]
		# Now we are performing, store the ID of the story and the first
		# piece ID.
		me.WriteKey("performing", str(stories[GetOptions()].index(l)))
		me.WriteKey("performing_id", "0")
		me.map.Message(me.x, me.y, 6, "\n{} says:\n*ahem*".format(me.name), COLOR_NAVY)
		# Start the first piece after a slight delay.
		me.CreateTimer(1, 1)

## Handle the timer events.
def timer():
	# Story piece ID.
	i = int(me.ReadKey("performing_id"))
	# The story being performed.
	story = stories[GetOptions()][int(me.ReadKey("performing"))][1]
	# Show the message piece to all nearby.
	me.map.Message(me.x, me.y, 6, "\n{} chants:\n{}".format(me.name, story[i]), COLOR_NAVY)
	# Advance the piece to show.
	i += 1

	# Ended the story?
	if i >= len(story):
		# Clear the fields.
		me.WriteKey("performing")
		me.WriteKey("performing_id")
		return

	# Re-create the timer.
	me.CreateTimer(6, 1)
	# Update the story piece ID.
	me.WriteKey("performing_id", str(i))

# Decide how to handle the event.
if GetEventNumber() == EVENT_SAY:
	main()
else:
	timer()

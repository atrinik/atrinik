## @file
## Script used for testing various Python functions in Developer
## Testmaps.

from Atrinik import *
import string

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

msg = WhatIsMessage().lower()
words = string.split(msg)

## Find any marked object.
marked = activator.FindMarkedObject()

# Tests for the python backend and the object model
if msg == "exception":
    me.SayTo(activator, "I will now raise an exception...")
    raise error("hello world")

elif msg == "constructor":
    me.SayTo(activator, "I will now try to create a Object object... (Should raise an exception)")
    test = Object()
    print test

elif msg == "str":
    me.SayTo(activator, "You are: " + str(activator) + ".\nI am " + str(me))

elif msg == "typesafe":
    me.SayTo(activator, "Testing type safety (wrapping strange objects)")
    other = me.map
    me.SayTo(other, "This will not be written nor crash the server")

# Test food-related things
elif msg == "food":
	me.SayTo(activator, "Your food was %d" % activator.food)
	activator.food = 500
	me.SayTo(activator, "Your stomach is filled again.")

elif msg == "food2":
	me.SayTo(activator, "Trying to overfill:")
	activator.food = 2000

# Misc tests
elif msg == "messaging":
    me.Say("This is a message for everybody on the map in a range of %d tiles" % MAP_INFO_NORMAL)
    me.SayTo(activator, "This is SayTo just for you")
    me.SayTo(activator, "This is SayTo again but in mode 1 (no xxx say:).", 1)
    activator.Write("This is a non-verbal message just for you in red color.", COLOR_RED)
    activator.map.Message(me.x, me.y, 8, "This is a map-wide Message() for everybody in a range of 8 tiles")
    me.Communicate("/kiss")
    me.Communicate("/hug " + activator.name)

elif msg == "matchstring":
    me.SayTo(activator, "match(foo, bar) = %d" % (MatchString("foo", "bar")))
    me.SayTo(activator, "match(foo, foo) = %d" % (MatchString("foo", "foo")))
    me.SayTo(activator, "match(foo, FOO) = %d" % (MatchString("foo", "FOO")))

elif msg == "getip":
    me.SayTo(activator, "Your IP is %s." % activator.GetIP())

elif words[0] == "skill":
    if len(words) == 1:
        me.SayTo(activator, "Say ^skill <skill>^, e.g. ^skill punching^")
    else:
		## Get the skill number.
        skillnr = GetSkillNr(words[1])
        me.SayTo(activator, "Checking on skill %s (nr %d)" % (words[1], skillnr))

        if not activator.DoKnowSkill(skillnr):
            me.SayTo(activator, "You do not know that skill!")
        else:
            exp = activator.GetSkill(TYPE_SKILL, skillnr).experience
            me.SayTo(activator, "Your experience in %s is %d" % (words[1], exp))
            activator.SetSkill(TYPE_SKILL, skillnr, 0, exp + 1000)
            me.SayTo(activator, "Tried to set it to %d" % (exp + 1000))
            me.SayTo(activator, "Your new experience is %d" % (activator.GetSkill(TYPE_SKILL, skillnr)).experience)

elif words[0] == "findplayer":
    if len(words) == 1:
        me.SayTo(activator, "Trying to find you. You can find another player by saying: ^findplayer NAME^")
        player = FindPlayer(activator.name)
    else:
        player = FindPlayer(words[1].title())

    if player == None:
        me.SayTo(activator, "Sorry, couldn't find that player.")
    else:
        me.SayTo(activator, "Found %s on the map %s." % (player.name, str(player.map)))

elif words[0] == "playerexists":
    if len(words) == 1:
        me.SayTo(activator, "Usage: ^playerexists NAME^")
    else:
		if not PlayerExists(words[1].title()):
			me.SayTo(activator, "%s does not exist." % words[1].title())
		else:
			me.SayTo(activator, "%s exists in the database!" % words[1].title())

elif words[0] == "setgender":
    if len(words) == 1:
        me.SayTo(activator, "Try ^setgender male^,\n^setgender female^, ^setgender both^ or\n^setgender neuter^.")
    else:
        me.SayTo(activator, "Setting your gender to %s." % words[1])

        if words[1] == "male":
            activator.SetGender(MALE)
        elif words[1] == "neuter":
            activator.SetGender(NEUTER)
        elif words[1] == "female":
            activator.SetGender(FEMALE)
        elif words[1] == "both":
            activator.SetGender(HERMAPHRODITE)

elif msg == "sound":
    me.SayTo(activator, "I can imitate a teleporter.")
    me.map.PlaySound(me.x, me.y, 32, 0)

# misc attributes:
elif words[0] == "alignment":
    align = activator.GetAlignmentForce()
    me.SayTo(activator, "Your old alignment was %s." % align.title)

    if len(words) == 1:
        me.SayTo(activator, "Try ^alignment ALIGNMENT^")
    else:
        me.SayTo(activator, "Setting your alignment to %s." % words[1])
        activator.SetAlignment(words[1])
        me.SayTo(activator, "Your new alignment is %s!" % activator.GetAlignmentForce().title)

elif words[0] == "experience":
    me.SayTo(activator, "Your overall exp is %d." % activator.experience)

elif words[0] == "direction":
    me.SayTo(activator, "You are facing direction %d." % activator.direction)

elif words[0] == "flags":
    conf = activator.f_confused

    if conf == 1:
        me.SayTo(activator, "You are confused. I'm removing it...")
        activator.f_confused = 0
    else:
        me.SayTo(activator, "You aren't confused. Fixing...")
        activator.f_confused = 1

elif words[0] == "archname":
    if marked == None:
        me.SayTo(activator, "Your archname is %s." % activator.GetArchName())
    else:
        me.SayTo(activator, "Your item is a %s." % marked.GetArchName())

# Test rank
elif msg == "rank1":
	me.SayTo(activator, "Setting rank to Mr...")
	activator.SetRank("Mr")

elif msg == "rank2":
	me.SayTo(activator, "Setting rank to President...")
	activator.SetRank("President")

elif msg == "enemy":
    enemy = activator.enemy

    if enemy == None:
        me.SayTo(activator, "You have no enemy.")
    else:
        me.SayTo(activator, "Your enemy is " + enemy.name)

        if enemy.enemy == None:
            me.SayTo(activator, "%s has no enemy." % enemy.name)
        else:
            me.SayTo(activator, "%s's enemy is %s" % (enemy.name, enemy.enemy.name))

# Test map object model, functions and object queries
elif msg == "map1":
    me.SayTo(activator, "We are now on map %s." % str(me.map))

elif msg == "map2":
    me.SayTo(activator, "This map (%s) is %d x %d" % (me.map.path, me.map.width, me.map.height))

elif msg == "map3":
    saywhat = "Object list for square (1, 1):"
    object = me.map.GetFirstObjectOnSquare(1, 1)

    while object != None:
        saywhat += "\n  " + str(object)
        object = object.above

    me.SayTo(activator, saywhat)

else:
    me.SayTo(activator,
        "Available tests:\n"
        "^exception^ ^constructor^ ^str^ ^typesafe^\n"
        "^food^ ^food2^\n"
		"^messaging^ ^getip^\n"
		"^rank1^ ^rank2^ ^enemy^\n"
        "^map1^ ^map2^ ^map3^\n"
        "^skill^ ^matchstring^\n"
        "^findplayer^ ^playerexists^ ^setgender^ ^sound^\n"
        "^flags^\n"
        "^alignment^ ^experience^ ^direction^")

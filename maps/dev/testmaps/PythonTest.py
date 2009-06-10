from Atrinik import *
import string

me = WhoAmI()
activator = WhoIsActivator()
msg = WhatIsMessage().lower()
words = string.split(msg)

marked = activator.FindMarkedObject()

# Tests for the python backend and the object model
if msg == 'exception':
    me.SayTo(activator, "I will now raise an exception...")
    raise error('hello world')
elif msg == 'constructor':
    me.SayTo(activator, "I will now try to create a Object object... (Should raise an exception)")
    test = Object()
    print test
elif msg == 'str':
    me.SayTo(activator, "You are: " + str(activator) + ".\nI am " + str(me))    	
elif msg == 'typesafe':
    me.SayTo(activator, "Testing type safety (wrapping strange objects)")    
#    other = WhoIsOther() # This is NULL for 'Say' scripts
    other = me.map
    me.SayTo(other, "This will not be written nor crash the server")        

# Test food-related things
elif msg == 'food':
	me.SayTo(activator,"Your food was " + str(activator.food))
	activator.food = 500
	me.SayTo(activator,'Your stomach is filled again.')
elif msg == 'food2':
	me.SayTo(activator,'Trying to overfill:')
	activator.food = 2000
	
# Misc tests	
elif msg == 'messaging':
    me.Say('This is a message for everybody on the map in a range of %d tiles' % MAP_INFO_NORMAL)
    me.SayTo(activator, 'This is SayTo just for you')
    me.SayTo(activator, 'This is SayTo again but in mode 1 (no xxx say:).', 1)
    activator.Write('This is a non-verbal message just for you in red color.', COLOR_RED)
    activator.map.Message(me.x, me.y, 8, 'This is a map-wide Message() for everybody in a range of 8 tiles')
    me.Communicate('/kiss')
    me.Communicate('/hug ' + activator.name)
elif msg == 'matchstring':
    me.SayTo(activator, "match(foo, bar) = %d" % (MatchString("foo", "bar")))
    me.SayTo(activator, "match(foo, foo) = %d" % (MatchString("foo", "foo")))
    me.SayTo(activator, "match(foo, FOO) = %d" % (MatchString("foo", "FOO")))
elif msg == 'getip':
    me.SayTo(activator, "Your IP is " + activator.GetIP())
elif words[0] == 'skill':
    if len(words) == 1:
        me.SayTo(activator, "Say ^skill <skill>^, e.g. ^skill punching^")
    else:
        skillnr = GetSkillNr(words[1])
        me.SayTo(activator, "Checking on skill %s (nr %d)" % (words[1], skillnr))
        if not activator.DoKnowSkill(skillnr):
            me.SayTo(activator, "You do not know that skill!")
        else:
            exp = activator.GetSkill(TYPE_SKILL, skillnr).experience
            me.SayTo(activator, "Your experience in %s is %d" % (words[1], exp))
            activator.SetSkill(TYPE_SKILL, skillnr, 0, exp + 1000) 
            me.SayTo(activator, "Tried to set it to %d" % (exp + 1000))            
            me.SayTo(activator, "Your new experience is %d" % (activator.GetSkill(TYPE_SKILL,skillnr)).experience) 
elif msg == 'god':
    # TODO: change to god when finished
    god = activator.GetGod()
    me.SayTo(activator, "Your current god is " + god)
#    if god == 'Eldath':
#        SetGod(activator, 'Snafu')
#    elif god == 'Snafu':
#        SetGod(activator, 'Eldath')
#    me.SayTo(activator, "Your new god is " + GetGod(activator))
elif words[0] == 'findplayer':
    if len(words) == 1:
        me.SayTo(activator, "Trying to find you. You can find another player by saying: ^findplayer NAME^")
        player = FindPlayer(activator.name)
    else:
        player = FindPlayer(words[1])
    if player == None:
        me.SayTo(activator, "Sorry, couldn't find " + plyname)
    else:        
        me.SayTo(activator, "Found " + player.name + " on the map " + str(player.map))
elif words[0] == 'setgender':
    if len(words) == 1:
        me.SayTo(activator, "Try ^setgender male^, ^setgender female^, ^setgender both^ or ^setgender neuter^")
    else:
        me.SayTo(activator, "Setting your gender to " + words[1])
        if(words[1] == 'male'):
            activator.SetGender(MALE)
        elif(words[1] == 'neuter'):
            activator.SetGender(NEUTER)
        elif(words[1] == 'female'):
            activator.SetGender(FEMALE)
        elif(words[1] == 'both'):
            activator.SetGender(HERMAPHRODITE)
    
elif msg == 'sound':
    me.SayTo(activator, "I can imitate a teleporter")
    me.map.PlaySound(me.x, me.y, 32, 0)

# Setting/Getting of stats
elif words[0] == 'stat':
    id = -1
    modded = ''
    
    if len(words) == 1:
        me.SayTo(activator, "I need a stat to change, e.g. ^stat int^.")        
    elif words[1] == 'con':
        id = CONSTITUTION
    elif words[1] == 'int':
        id = INTELLIGENCE
    elif words[1] == 'str':
        id = STRENGTH
    elif words[1] == 'pow':
        id = POWER
    elif words[1] == 'wis':
        id = WISDOM
    elif words[1] == 'dex':
        id = DEXTERITY
    elif words[1] == 'cha':
        id = CHARISMA
    elif words[1] == 'hp':
        id = HITPOINTS
    elif words[1] == 'sp':
        id = SPELLPOINTS
    elif words[1] == 'grace':
        id = GRACE
    elif words[1] == 'maxhp': # set shouldn't work...
        id = MAX_HITPOINTS
    elif words[1] == 'maxsp': # set shouldn't work...
        id = MAX_SPELLPOINTS
    elif words[1] == 'maxgrace': # set shouldn't work...
        id = MAX_GRACE
    elif words[1] == 'ac':# set shouldn't work...
        id = ARMOUR_CLASS
    elif words[1] == 'wc': # set shouldn't work...
        id = WEAPON_CLASS
    elif words[1] == 'damage': # set shouldn't work...
        id = DAMAGE
    elif words[1] == 'luck': # set shouldn't work...
        id = LUCK
    else:
        me.SayTo(activator, "Unknown stat: " + words[1])                

    if id != -1:
        if id == CONSTITUTION or id == INTELLIGENCE or id == STRENGTH or id == POWER or id == WISDOM or id == DEXTERITY or id == CHARISMA: 
            old = GetUnmodifiedAttribute(activator, id)
            modded = " (modded to %d)" % (activator.id)
        else:            
            old = activator.GetAttribute(id)
        me.SayTo(activator, "Your current %s%s stat: %d" % (words[1], modded, old))
        me.SayTo(activator, "Increasing to %d" % (old+1))                
        activator.id = old+1

# misc attributes:
elif words[0] == 'alignment':
    align = activator.GetAlignmentForce()
    me.SayTo(activator, "Your old alignment was " + align.title)
    if len(words) == 1:
        me.SayTo(activator, "Try ^alignment ALIGNMENT^")
    else:
        me.SayTo(activator, "Setting your alignment to " + words[1])
        activator.SetAlignment(words[1])
        me.SayTo(activator, "Your new alignment is " + activator.GetAlignmentForce().title)
elif words[0] == 'experience':
    me.SayTo(activator, "Your overall exp is " + str(activator.experience))
elif words[0] == 'direction':
    me.SayTo(activator, "You are facing direction " + str(activator.direction))
elif words[0] == 'player_force':
    me.SayTo(activator, "Adding a temporary boosting force to you")
    force = activator.CreatePlayerForce("test force", 5)
    if force != None:
        force.intelligence = 1
        force.damage = 10
        force.armour_class = 10
        force.weapon_class = 10
        force.luck = 10
        force.max_hitpoints = 10
        force.max_spellpoints = 10
        force.max_grace = 10 
elif words[0] == 'flags':
    conf = activator.f_confused
    if conf == 1:
        me.SayTo(activator, "You are confused. I'm removing it...")
        activator.f_confused = 0
    else:
        me.SayTo(activator, "You aren't confused. Fixing...")
        activator.f_confused = 1
elif words[0] == 'archname':
    if marked == None:
        me.SayTo(activator, "Your archname is " + activator.GetArchName())
    else:
        me.SayTo(activator, "Your item is a " + marked.GetArchName())
    
# Some object-property-changing tests 
# TODO: create more tests for the "inventory-update" problem    
elif msg == 'weight':
    if marked == None:
        me.SayTo(activator, "You must mark an item first.")
    else:
        me.SayTo(activator, "Your item weighted " + str(marked.weight) + "g")
        marked.weight -= 5
        me.SayTo(activator, "Your item now weights " + str(marked.weight) + "g")
elif msg == 'quantity':
    if marked == None:
        me.SayTo(activator, "You must mark an item first.")
    else:
        me.SayTo(activator, "There was " + str(marked.quantity) + " items")
        marked.quantity += 1
        me.SayTo(activator, "Now there are " + str(marked.quantity))
elif msg == 'value':
    if marked == None:
        me.SayTo(activator, "You must mark an item first.")
    else:
        me.SayTo(activator, "The old value of your object was " + str(marked.value))
        marked.value += 100
        me.SayTo(activator, "The new value of your object is " + str(marked.value))
elif msg == 'cursed':
    if marked == None:
        me.SayTo(activator, "You must mark an item first.")
    else:
        if marked.f_cursed:
            marked.f_cursed = 0
            me.SayTo(activator, "Your item is now uncursed")
        else:			
            marked.f_cursed = 1
            me.SayTo(activator, "Your item is now cursed")
elif words[0] == 'name':
    if marked == None:
        me.SayTo(activator, "You must mark an item first.")
    else:
        if len(words) == 1:
            me.SayTo(activator, "Say ^name NEWNAME^")
        else:			
            marked.name = words[1]
            me.SayTo(activator, "Your item is now called " + marked.name)
elif words[0] == 'title':
    if marked == None:
        me.SayTo(activator, "You must mark an item first.")
    else:
        if len(words) == 1:
            me.SayTo(activator, "Say ^title NEWNAME^")
        else:			
            if marked.title == None:
                me.SayTo(activator, "Your item had no title ")
            else:                
                me.SayTo(activator, "Your item had the title " + marked.title)
            marked.title = words[1]
            me.SayTo(activator, "Your item is now titled " + marked.title)
elif words[0] == 'slaying':
    if marked == None:
        me.SayTo(activator, "You must mark an item first.")
    else:
        if len(words) == 1:
            me.SayTo(activator, "Say ^slaying NEWNAME^")
        else:			
            if marked.slaying == None:
                me.SayTo(activator, "Your item had no slaying")
            else:                
                me.SayTo(activator, "Your item slays " + marked.slaying)
            marked.slaying = words[1]
            me.SayTo(activator, "Your item now slays " + marked.slaying)

# Test rank	
elif msg == 'rank1':
	me.SayTo(activator, 'Setting rank to "Mr"...')
	activator.SetRank('Mr')
elif msg == 'rank2':
	me.SayTo(activator, 'Setting rank to "President"...')
	activator.SetRank('President')

# Test cloning
elif msg == 'clone':
    me.SayTo(activator, 'Meet your evil twin')
    clone = activator.Clone(CLONE_WITHOUT_INVENTORY)
    clone.TeleportTo(me.map.path, 10,10)
    clone.f_friendly = 0
    clone.f_monster = 1
    clone.max_hitpoints = 1
    clone.level = 1
    clone.name = clone.name + " II"
    clone.Fix()
elif msg == 'enemy':
    enemy = activator.enemy
    if enemy == None:
        me.SayTo(activator, 'You have no enemy currently')
    else:
        me.SayTo(activator, 'Your enemy is ' + enemy.name)
        if enemy.enemy == None:
            me.SayTo(activator, enemy.name + " has no enemy.")
        else:
            me.SayTo(activator, enemy.name + "'s enemy is " + enemy.enemy.name)

    
# Test SetFace
elif msg == 'setface':
    # FIXME: Sets animation, not face
    me.SayTo(activator, "Changing appearance")
    SetFace(me, "giant_hill")
    
# Test player invisibility
elif msg == 'invisible':
    # FIXME: Seems to have some problems if the player is made invisible (UP_INV_XXX flag ????)
	if activator.f_is_invisible:
		me.SayTo(activator, "Where are you? I can't see you! Aha, making you visible!")
		activator.f_is_invisible = 0
	else:
		me.SayTo(activator, "Making you invisible...")
		activator.f_is_invisible = 1

# Test some crash-prone object functions
elif msg == 'setposition1':
    me.SayTo(activator, "Trying to move one step west")
    me.SetPosition(me.x-1, me.y)
elif msg == 'setposition2':
    me.SayTo(activator, "Dropping a note one step to the west")
    obj = me.CreateObjectInside("note", 0, 1)
    me.Drop("note")
    obj.SetPosition(me.x - 1 ,me.y)
elif msg == 'setposition3':
    me.SayTo(activator, "Putting a note one step to the west (probably won't work...)")
    obj = me.CreateObjectInside("note", 0, 1)
    obj.SetPosition(me.x-1, me.y)

# Pickup and drop objects
elif msg == 'drop1':
    me.SayTo(activator, "Dropping a note")
    me.CreateObjectInside("note", 0, 1)
    me.Drop("note")
elif msg == 'drop2':
    me.SayTo(activator, "Oops, you dropped a note")
    activator.CreateObjectInside("note", 0, 1)
    activator.Drop("note")

elif msg == 'pickup1':
    tmp = me.map.GetFirstObjectOnSquare(me.x, me.y) 
    while tmp != None:
        if tmp.name == 'note':
            break
        tmp = tmp.above
    if tmp == None:
        me.SayTo(activator, "Sorry, couldn't find anything to pick up. Try using ^drop1^ first.")
    else:   
        me.PickUp(tmp)                
elif msg == 'pickup2':
    tmp = activator.map.GetFirstObjectOnSquare(activator.x, activator.y) 
    while tmp != None:
        if tmp.name == 'note':
            break
        tmp = tmp.above

    if tmp == None:
        me.SayTo(activator, "Nothing to pick up. Try using ^drop2^ first.")
    else:   
        activator.PickUp(tmp)        
    
elif words[0] == "getattribute":
    me.SayTo(activator, "Name: " + str(activator.name))
    me.SayTo(activator, "Race: " + str(activator.race))
    me.SayTo(activator, "Weight: " + str(activator.weight))
    me.SayTo(activator, "Exp: " + str(activator.experience))
    me.SayTo(activator, "Speed: " + str(activator.speed))
elif words[0] == "setattribute":
    activator.title = "the elite"
    activator.speed = 2.0 # Shouldn't work on player...

# Test map object model, functions and object queries
elif msg == 'map1':
    me.SayTo(activator, "We are now on map " + str(me.map))
elif msg == 'map2':
    me.SayTo(activator, "This map (%s) is %d x %d" % (me.map.path, me.map.width, me.map.height))
elif msg == 'map3':
    saywhat = "Object list for square (1,1):"
    object = me.map.GetFirstObjectOnSquare(1,1) 
    while object != None:
        saywhat += "\n  " + str(object)
        object = object.above
    me.SayTo(activator, saywhat)
elif msg == 'createobject':
    note = me.map.CreateObject("note", me.x-1, me.y-1)
    note.message = "I was created out here"
    
else:
    me.SayTo(activator, 
        "Available tests:\n"
        "^exception^ ^constructor^ ^str^ ^typesafe1^\n"
        "^food^ ^food2^\n"
		"^invisible^ ^messaging^ ^getip^\n"
		"^rank1^ ^rank2^ ^clone^ ^enemy^\n"
        "^map1^ ^map2^ ^map3^ ^createobject^\n"
		"^value^ ^cursed^ ^quantity^ ^weight^\n"
        "^name^ ^title^ ^slaying^ ^archname^\n"
        "^skill^ ^matchstring^ ^god^\n"
        "^findplayer^ ^setgender^ ^sound^\n"
        "^player_force^ ^flags^\n"
        "^setposition1^ ^setposition2^ ^setposition3^\n"
        "^drop1^ ^pickup1^ ^drop2^ ^pickup2^\n"
        "^alignment^ ^experience^ ^direction^\n"
        "^stat {str|dex|con|pow|wis|cha|hp|sp|grace|maxhp|maxsp|maxgrace|ac|wc|luck}^\n"
        "^setface^ ^getattribute^ ^setattribute^")

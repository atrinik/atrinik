## @file
## Implements Ruri, the fairy helper's dialog.

from Interface import Interface

msg = msg.lower()

inf = Interface(activator, me)

# used in 2 places.  Easier to add options this way.
def main_menu():
	inf.add_link("<a=:book>Find a book.</a>")
	inf.add_link("<a=:town>Ask about the town.</a>")
	inf.add_link("<a=:librarian>Ask about the librarian.</a>")
	inf.add_link("<a=close:>Goodbye.</a>")

def main():
	if msg == "hello" or msg == "hi" or msg == "hey":
		inf.add_msg("Hi, there {}.  What's up?  Can I help you find a book?  Maybe you want to ask about other information?".format(activator.name))
		main_menu()
	elif msg == "main_menu":
		inf.add_msg("Was there anything else you wanted to talk about {}?".format(activator.name))
		main_menu()
	elif msg == "book":
		inf.add_msg("All right.  What kind of book are you looking for?  You are probably interested in the Eastern Project series, right?  That's what everyone else seems to be reading these days.")
		inf.add_msg("[NOTE: Not all available series are listed.]", COLOR_RED)
		inf.add_link("<a=:touhou>Ask about the Eastern Project.</a>")
		inf.add_link("<a=:main_menu>Ask about something else.</a>")
	elif msg == "town":
		inf.add_msg("Try asking me some stuff about this town.  I might be able to help.")
		inf.add_link("<a=:centennial>Ask why the town is called Centennial.</a>")		
		inf.add_link("<a=:witch>Ask about the witches.</a>")
		inf.add_link("<a=:main_menu>Ask about something else.</a>")
	elif msg == "librarian" or msg == "Lindy":
		inf.add_msg("She's nice enough.  Was there anything else?")
		inf.add_link("<a=:main_menu>Ask about something else.</a>")
	elif msg == "centennial":
		inf.add_msg("The name of this town is also a bit of an in-joke.  If you are really that concerned about it, it's found in the 6th title of the \"Eastern Project\" series in the \"Extra\" section.")
		inf.add_link("<a=:touhou>Ask about the Eastern Project.</a>")
		inf.add_link("<a=:main_menu>Ask about something else.</a>")
	elif msg == "the strongest":
		inf.add_msg("Nope, sorry.  Wrong fairy.  The tomboyish fairy who says that is on the river east of Aris in the early mornings.")
		inf.add_link("<a=:tomboyish fairy>Ask about the tomboyish fairy.</a>")
		inf.add_link("<a=:main_menu>Ask about something else.</a>")
	elif msg == "touhou":
		inf.add_msg("They're kinda cool stories with lots of magic, but some people seem a bit too hyped up about them.  The name is sort of a translation from the original language.  Anyway, they are over in the Fantasy aisle.")
		inf.add_link("<a=:th_chars>Ask Eastern Project characters.</a>".format(me.name))
		inf.add_link("<a=:magic>Must be popular because of all the magic.</a>")
		inf.add_link("<a=:main_menu>Ask about something else.</a>")
	elif msg == "th_chars":
		inf.add_msg("I suppose it's a bit cliche but I'm sort of partial to the fairies like Cirno and Daiyousei, myself.  Then there's the other characters like Marisa, Reimu, Sanae, Patchouli and Alice that seem to be quite popular around the town gauging by the looks of it.")
		inf.add_link("<a=:alice>Ask about Alice.</a>")
		inf.add_link("<a=:cirno>Ask about Cirno.</a>")
		inf.add_link("<a=:daiyousei>Ask about Daiyousei.</a>")
		inf.add_link("<a=:marisa>Ask about Marisa.</a>")
		inf.add_link("<a=:patchouli>Ask about Patchouli.</a>")
		inf.add_link("<a=:reimu>Ask about Reimu.</a>")
		inf.add_link("<a=:sanae>Ask about Sanae.</a>")
		inf.add_link("<a=:main_menu>Ask about something else.</a>")
	elif msg == "witch":
		inf.add_msg("Yes.  This school is dedicated to teaching the fine art of witchcraft.  You know, hexes, potions, magic?")
		inf.add_link("<a=:magic>Magic?</a>")		
	elif msg == "magic":
		inf.add_msg("I guess that's what happens in a town of magical girls... Er.  I mean, witches.")
		inf.add_link("<a=:magical girls>Huh?  Magical girls?</a>")
	elif msg == "magical girls":
		inf.add_msg("Interesting story about that.  In some languages, it's possible to change \"magical girl\" into \"witch\" by removing 2 symbols.  If you think about it, it kind of seems that seems to be a theme of our town.")
		inf.add_msg("{} winks at you like this is a secret.".format(me.name), COLOR_YELLOW)
		inf.add_link("<a=:main_menu>Ask about something else.</a>")
	elif msg == "cirno":
		inf.add_msg("Yeah, she's an fairy that lives by the Misty Lake near the Scarlet Devil Mansion.  Of course, it's a bit of a thing to depict her as though she's dumb.  Personally, I don't think Cirno is all *THAT* dumb even if she is a bit childish.  She did manage to star in her own title from the series and even managed to beat Marisa once.")
		inf.add_link("<a=:th_chars>Ask about the other characters.</a>")
		inf.add_link("<a=:main_menu>Ask about something else.</a>")
	elif msg == "daiyousei":
		inf.add_msg("Yeah, she's an fairy that lives by the Misty Lake near the Scarlet Devil Mansion.  She's not really mentioned much except for in the 6th and 12.8th titles, but I kind of like that fan series that depicts her as cool and smart.")
		inf.add_link("<a=:nekokayou>Fan series.</a>")
		inf.add_link("<a=:th_chars>Ask about the other characters.</a>")
		inf.add_link("<a=:main_menu>Ask about something else.</a>")
	elif msg == "alice":
		inf.add_msg("She is a youkai magician with lots of enchanted dolls.  You should be able to recognize people who like her since they dress up with the short blonde hair and blue clothes.")
		inf.add_link("<a=:th_chars>Ask about the other characters.</a>")
		inf.add_link("<a=:main_menu>Ask about something else.</a>")
	elif msg == "marisa":
		inf.add_msg("She is klepto witch who has a thing for beam spells.  She's basically one of the main characters and pretty popular.  You can recognize folks that like her by their long blonde hair and black-white witch motif.")
		inf.add_link("<a=:witch>Witch?  Isn't that what this school teaches?</a>")
		inf.add_link("<a=:th_chars>Ask about the other characters.</a>")
		inf.add_link("<a=:main_menu>Ask about something else.</a>")
	elif msg == "patchouli":
		inf.add_msg("She is a youkai magician who lives in the Scarlet Devil Mansion library.  For obvious reasons, the librarian is a bit of a fan.  They tend to dress up in the lavender night robe, crescent moon hat with the purple hair.  Obviously she's not to be confused with the aromatic herb.")
		inf.add_link("<a=:librarian>Ask about the librarian.</a>")
		inf.add_link("<a=:th_chars>Ask about the other characters.</a>")
		inf.add_link("<a=:main_menu>Ask about something else.</a>")
	elif msg == "reimu":
		inf.add_msg("She is a shrine maiden and pretty much the main character except when Marisa or one of the other characters is the focus of the story.  She's kind of easy to irritate and a bit touchy about not getting any donations.  You can recognize the fans because they wear the red-white motif.")
		inf.add_link("<a=:th_chars>Ask about the other characters.</a>")
		inf.add_link("<a=:main_menu>Ask about something else.</a>")
	elif msg == "sanae":
		inf.add_msg("She is a shrine maiden of a rival shrine to Reimu's.  You can recognize the people who like her by their blue skirt, white top and green hair.")
		inf.add_link("<a=:th_chars>Ask about the other characters.</a>")
		inf.add_link("<a=:main_menu>Ask about something else.</a>")
	elif msg == "nekokayou":
		inf.add_msg("I think it's called Eastern Project \":3\".  I don't think we have a copy at the moment, though.")
		inf.add_link("<a=:main_menu>Ask about something else.</a>")
	# BONUS dialog options if user figures out the hints from the general theme...
	elif msg == "baka" or msg == "idiot" or msg == "nineball" or msg == "marukyuu" or msg == "(9)":
		inf.add_msg("{} looks insulted.".format(me.name), COLOR_YELLOW)
		inf.add_msg("Hey, now!  Don't underestimate me.  Just because some fairies are stupid doesn't mean we all are.")
		inf.add_link("<a=:main_menu>Ask about something else.</a>")
	elif msg == "tomboyish fairy":
		inf.add_msg("Yeah, that ice fairy by the lake has read way too many of those Eastern Project fan stories and now she thinks she's Cirno.")
		inf.add_link("<a=:cirno>Who is Cirno?</a>")
		inf.add_link("<a=:touhou>Ask about the Eastern Project.</a>")
		inf.add_link("<a=:main_menu>Ask about something else.</a>")
	elif msg == "ruri hoshino" or msg == "nadesico":
		inf.add_msg("Hmm... Interesting.  I didn't realize someone would ever guess my namesake.  It's a pretty obscure series here in this town.")
		inf.add_link("<a=:town>Ask about the town.</a>")
		inf.add_link("<a=:main_menu>Ask about something else.</a>")
	elif msg == "ahriman's prophecy":
		inf.add_msg("Good story.  Apparently the town founders liked it.  It's kind of why they founded the town where they did.")
		inf.add_link("<a=:town>Ask about the town.</a>")
		inf.add_link("<a=:main_menu>Ask about something else.</a>")

main()
inf.finish()

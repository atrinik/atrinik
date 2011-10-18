## @file
## Implements Ruri, the fairy helper's dialog.

from Interface import Interface

msg = msg.lower()

inf = Interface(activator, me)

# used in 2 places.  Easier to add options this way.
def main_menu():
	inf.add_link("Find a book.", dest = "book")
	inf.add_link("Ask about the town.", dest = "town")
	inf.add_link("Ask about the librarian.", dest = "librarian")
	inf.add_link("Goodbye.", action = "close")

def main():
	if msg == "hello":
		inf.add_msg("Hi, there {}.  What's up?  Can I help you find a book?  Maybe you want to ask about other information?".format(activator.name))
		main_menu()
	elif msg == "main_menu":
		inf.add_msg("Was there anything else you wanted to talk about {}?".format(activator.name))
		main_menu()
	elif msg == "book":
		inf.add_msg("All right.  What kind of book are you looking for?  You are probably interested in the Eastern Project series, right?  That's what everyone else seems to be reading these days.")
		inf.add_msg("[NOTE: Not all available series are listed.]", COLOR_RED)
		inf.add_link("Ask about the Eastern Project.", dest = "touhou")
		inf.add_link("Ask about something else.", dest = "main_menu")
	elif msg == "town":
		inf.add_msg("Try asking me some stuff about this town.  I might be able to help.")
		inf.add_link("Ask why the town is called Centennial.", dest = "centennial")
		inf.add_link("Ask about the witches.", dest = "witch")
		inf.add_link("Ask about something else.", dest = "main_menu")
	elif msg == "librarian" or msg == "lindy":
		inf.add_msg("She's nice enough.  Was there anything else?")
		inf.add_link("Ask about something else.", dest = "main_menu")
	elif msg == "centennial":
		inf.add_msg("The name of this town is also a bit of an in-joke.  If you are really that concerned about it, it's found in the 6th title of the \"Eastern Project\" series in the \"Extra\" section.")
		inf.add_link("Ask about the Eastern Project.", dest = "touhou")
		inf.add_link("Ask about something else.", dest = "main_menu")
	elif msg == "the strongest":
		inf.add_msg("Nope, sorry.  Wrong fairy.  The tomboyish fairy who says that is on the river east of Aris in the early mornings.")
		inf.add_link("Ask about the tomboyish fairy.", dest = "tomboyish fairy")
		inf.add_link("Ask about something else.", dest = "main_menu")
	elif msg == "touhou":
		inf.add_msg("They're kinda cool stories with lots of magic, but some people seem a bit too hyped up about them.  The name is sort of a translation from the original language.  Anyway, they are over in the Fantasy aisle.")
		inf.add_link("Ask Eastern Project characters.", dest = "th_chars")
		inf.add_link("Must be popular because of all the magic.", dest = "magic")
		inf.add_link("Ask about something else.", dest = "main_menu")
	elif msg == "th_chars":
		inf.add_msg("I suppose it's a bit cliche but I'm sort of partial to the fairies like Cirno and Daiyousei, myself.  Then there's the other characters like Marisa, Reimu, Sanae, Patchouli and Alice that seem to be quite popular around the town gauging by the looks of it.")
		inf.add_link("Ask about Alice.", dest = "alice")
		inf.add_link("Ask about Cirno.", dest = "cirno")
		inf.add_link("Ask about Daiyousei.", dest = "daiyousei")
		inf.add_link("Ask about Marisa.", dest = "marisa")
		inf.add_link("Ask about Patchouli.", dest = "patchouli")
		inf.add_link("Ask about Reimu.", dest = "reimu")
		inf.add_link("Ask about Sanae.", dest = "sanae")
		inf.add_link("Ask about something else.", dest = "main_menu")
	elif msg == "witch":
		inf.add_msg("Yes.  This school is dedicated to teaching the fine art of witchcraft.  You know, hexes, potions, magic?")
		inf.add_link("Magic?", dest = "magic")
	elif msg == "magic":
		inf.add_msg("I guess that's what happens in a town of magical girls... Er.  I mean, witches.")
		inf.add_link("Huh?  Magical girls?", dest = "magical girls")
	elif msg == "magical girls":
		inf.add_msg("Interesting story about that.  In some languages, it's possible to change \"magical girl\" into \"witch\" by removing 2 symbols.  If you think about it, it kind of seems that seems to be a theme of our town.")
		inf.add_msg("{} winks at you like this is a secret.".format(me.name), COLOR_YELLOW)
		inf.add_link("Ask about something else.", dest = "main_menu")
	elif msg == "cirno":
		inf.add_msg("Yeah, she's an fairy that lives by the Misty Lake near the Scarlet Devil Mansion.  Of course, it's a bit of a thing to depict her as though she's dumb.  Personally, I don't think Cirno is all *THAT* dumb even if she is a bit childish.  She did manage to star in her own title from the series and even managed to beat Marisa once.")
		inf.add_link("Ask about the other characters.", dest = "th_chars")
		inf.add_link("Ask about something else.", dest = "main_menu")
	elif msg == "daiyousei":
		inf.add_msg("Yeah, she's an fairy that lives by the Misty Lake near the Scarlet Devil Mansion.  She's not really mentioned much except for in the 6th and 12.8th titles, but I kind of like that fan series that depicts her as cool and smart.")
		inf.add_link("Fan series.", dest = "nekokayou")
		inf.add_link("Ask about the other characters.", dest = "th_chars")
		inf.add_link("Ask about something else.", dest = "main_menu")
	elif msg == "alice":
		inf.add_msg("She is a youkai magician with lots of enchanted dolls.  You should be able to recognize people who like her since they dress up with the short blonde hair and blue clothes.")
		inf.add_link("Ask about the other characters.", dest = "th_chars")
		inf.add_link("Ask about something else.", dest = "main_menu")
	elif msg == "marisa":
		inf.add_msg("She is klepto witch who has a thing for beam spells.  She's basically one of the main characters and pretty popular.  You can recognize folks that like her by their long blonde hair and black-white witch motif.")
		inf.add_link("Witch?  Isn't that what this school teaches?", dest = "witch")
		inf.add_link("Ask about the other characters.", dest = "th_chars")
		inf.add_link("Ask about something else.", dest = "main_menu")
	elif msg == "patchouli":
		inf.add_msg("She is a youkai magician who lives in the Scarlet Devil Mansion library.  For obvious reasons, the librarian is a bit of a fan.  They tend to dress up in the lavender night robe, crescent moon hat with the purple hair.  Obviously she's not to be confused with the aromatic herb.")
		inf.add_link("Ask about the librarian.", dest = "librarian")
		inf.add_link("Ask about the other characters.", dest = "th_chars")
		inf.add_link("Ask about something else.", dest = "main_menu")
	elif msg == "reimu":
		inf.add_msg("She is a shrine maiden and pretty much the main character except when Marisa or one of the other characters is the focus of the story.  She's kind of easy to irritate and a bit touchy about not getting any donations.  You can recognize the fans because they wear the red-white motif.")
		inf.add_link("Ask about the other characters.", dest = "th_chars")
		inf.add_link("Ask about something else.", dest = "main_menu")
	elif msg == "sanae":
		inf.add_msg("She is a shrine maiden of a rival shrine to Reimu's.  You can recognize the people who like her by their blue skirt, white top and green hair.")
		inf.add_link("Ask about the other characters.", dest = "th_chars")
		inf.add_link("Ask about something else.", dest = "main_menu")
	elif msg == "nekokayou":
		inf.add_msg("I think it's called Eastern Project \":3\".  I don't think we have a copy at the moment, though.")
		inf.add_link("Ask about something else.", dest = "main_menu")
	# BONUS dialog options if user figures out the hints from the general theme...
	elif msg == "baka" or msg == "idiot" or msg == "nineball" or msg == "marukyuu" or msg == "(9)":
		inf.add_msg("{} looks insulted.".format(me.name), COLOR_YELLOW)
		inf.add_msg("Hey, now!  Don't underestimate me.  Just because some fairies are stupid doesn't mean we all are.")
		inf.add_link("Ask about something else.", dest = "main_menu")
	elif msg == "tomboyish fairy":
		inf.add_msg("Yeah, that ice fairy by the lake has read way too many of those Eastern Project fan stories and now she thinks she's Cirno.")
		inf.add_link("Who is Cirno?", dest = "cirno")
		inf.add_link("Ask about the Eastern Project.", dest = "touhou")
		inf.add_link("Ask about something else.", dest = "main_menu")
	elif msg == "ruri hoshino" or msg == "nadesico":
		inf.add_msg("Hmm... Interesting.  I didn't realize someone would ever guess my namesake.  It's a pretty obscure series here in this town.")
		inf.add_link("Ask about the town.", dest = "town")
		inf.add_link("Ask about something else.", dest = "main_menu")
	elif msg == "ahriman's prophecy":
		inf.add_msg("Good story.  Apparently the town founders liked it.  It's kind of why they founded the town where they did.")
		inf.add_link("Ask about the town.", dest = "town")
		inf.add_link("Ask about something else.", dest = "main_menu")

main()
inf.finish()

## @file
## Implements the bored student witch's responses.

from Interface import Interface

msg = msg.lower()

inf = Interface(activator, me)

def main():
	options = GetOptions()
	if options == "classTime":
		if msg == "hello":
			inf.add_msg("She only summons the boring animals!")
		elif msg == "alice" or msg == "margatroid" or msg == "alice margatroid":
			inf.add_msg("Now she can summon exploding dolls!  Awesome!")
	elif options == "clubTime":
		if msg == "hello":
			inf.add_msg("Being on the student council is hard work.")
		elif msg == "alice" or msg == "margatroid" or msg == "alice margatroid":
			inf.add_msg("Maybe later, I've got a student council to run.")
	elif options == "nightTime":
		if msg == "hello":
			inf.add_msg("Hi, what's up {}?".format(activator.name))
		elif msg == "alice" or msg == "margatroid" or msg == "alice margatroid":
			inf.add_msg("Ooh, she's the best.  I've even got a Shanghai doll!")
			inf.add_link("Shanghai doll?  Why?", dest = "shanghai doll")
		elif msg == "shanghai doll":
			inf.add_msg("Yeah, because they're Alice's favorite.")

main()
inf.finish()

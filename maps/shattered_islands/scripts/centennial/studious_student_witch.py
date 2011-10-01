## @file
## Implements the studious student witch's responses.

from Interface import Interface

msg = msg.lower()

inf = Interface(activator, me)

def main():
	options = GetOptions()
	if options == "classTime":
		if msg == "hello"
			inf.add_msg("Hey.  Listening to the teacher...")
		elif msg == "alice" or msg == "margatroid" or msg == "alice margatroid":
			inf.add_msg("Maybe later, I've really gotta pass this test.")
	elif options == "clubTime":
		if msg == "hello" or msg == "alice" or msg == "margatroid" or msg == "alice margatroid":
			inf.add_msg("Too busy now.  Trying to win against this cheater.")
			inf.add_link("Who is cheating?")
		elif msg == "who is cheating?":
			inf.set_title("smug student witch")
			inf.set_icon("witch1.131")
			inf.add_msg("It ain't cheating, I'm just playing creatively.  Haha.~")
			inf.add_link("&lt;end&gt;", action = "close")

main()
inf.finish()

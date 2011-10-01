## @file
## Implements the studious student witch's card game responses.

from Interface import Interface

msg = msg.lower()

inf = Interface(activator, me)

def main():
	if msg == "hello" or msg == "alice" or msg == "margatroid" or msg == "alice margatroid":
		inf.add_msg("Too busy now.  Trying to win against this cheater.")
		inf.add_link("Who is cheating?")
	elif msg == "who is cheating?":
		inf.set_title("smug student witch")
		inf.set_icon("witch1.131")
		inf.add_msg("It ain't cheating, I'm just playing creatively.  Haha.~")
		inf.add_link("<a=close:>&lt;end&gt;</a>")

main()
inf.finish()

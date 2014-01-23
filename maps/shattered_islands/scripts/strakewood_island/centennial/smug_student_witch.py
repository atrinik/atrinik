## @file
## Implements the smug student witch's responses.

from Interface import Interface

msg = msg.lower()

inf = Interface(activator, me)

def main():
    options = GetOptions()
    if options == "classTime":
        if msg == "hello":
            inf.add_msg("Zzzzzzzz.....")
        elif msg == "marisa" or msg == "kirisame" or msg == "marisa kirisame":
            inf.add_msg("{} mumbles \"Love Sign: Master Spark\".".format(me.name), COLOR_YELLOW)
    elif options == "clubTime":
        if msg == "hello":
            inf.add_msg("Heh.  This sucker keeps falling for the same trick.")
            inf.add_link("Are you cheating?")
        elif msg == "are you cheating?":
            inf.add_msg("It ain't cheating, I'm just playing creatively.  Haha.~")
        elif msg == "marisa" or msg == "kirisame" or msg == "marisa kirisame":
            inf.add_msg("Marisa's awesome isn't she?")

main()
inf.finish()

## @file
## Implements the bookish student witch's responses.

from Interface import Interface

msg = msg.lower()

inf = Interface(activator, me)

def main():
    options = GetOptions()
    if options == "classTime":
        if msg == "hello":
            inf.add_msg("Shh, I'm trying to learn here.")
        elif msg == "patchouli" or msg == "patchouli knowledge":
            inf.add_msg("C'mon.  Let me listen to the teacher, man...")
    elif options == "clubTime" or options == "nightTime":
        if msg == "hello":
            inf.add_msg("Shh, I'm trying to study.")
        elif msg == "patchouli" or msg == "patchouli knowledge":
            inf.add_msg("Hmm.  I'm in a good mood today.  Maybe I'll just have to show you \"Sun Sign: Royal Flare...\"")
            inf.add_link("Ooh!  Royal Flare?  What's that?", dest = "royal flare")
        elif msg == "royal flare":
            inf.add_msg("Grr... take a hint or I'll show you to the basement.  Kihihihihihi.")
            inf.add_link("Cool!  What's in the basement?", dest = "basement")
        elif msg == "basement":
            inf.add_msg("Geez.  You must be as dumb as that fairy.")
            inf.add_link("Hey!  Wait, who is \"that fairy?\"", dest = "fairy")
        elif msg == "fairy" or msg == "cirno":
            inf.add_msg("C'mon.  Let me study, man...")

main()
inf.finish()

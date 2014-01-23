## @file
## Implements the fairy-crazed student witch's responses.

from Interface import Interface

msg = msg.lower()

inf = Interface(activator, me)

def main():
    options = GetOptions()
    if options == "classTime":
        if msg == "hello":
            inf.add_msg("Whoa.  Hey!  Maybe I can learn to summon a fairy!")
            inf.add_link("Ask about fairies.", dest = "fairy")
        elif msg == "fairy":
            inf.add_msg("Fairies are kinda cute.")
        elif msg == "reimu" or msg == "hakurei" or msg == "reimu hakurei":
            inf.add_msg("Neko Miko Reimu.  Aishite'ru...")
    elif options == "clubTime":
        if msg == "hello":
            inf.add_msg("Aww, maybe tomorrow I'll learn how to summon a fairy.")
            inf.add_link("Ask about fairies.", dest = "fairy")
        elif msg == "fairy":
            inf.add_msg("Fairies are kinda cute.")
        elif msg == "reimu" or msg == "hakurei" or msg == "reimu hakurei":
            inf.add_msg("Neko Miko Reimu.  Aishite'ru...")

main()
inf.finish()

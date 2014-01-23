## @file
## Script for Geoffrey Charob in Asteria.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("Welcome to Charob Beer Inc.")
        inf.add_msg("I'm Geoffrey Charob, the president and founder of Charob Beer Inc.!")
        inf.add_msg("We produce the world famous Charob Beer, although for some reason my shipments haven't been going out on time, and I have my suspicions about a certain lazy worker...")
        inf.add_link("Tell me more about Charob Beer.", dest = "tellcharob")
        inf.add_link("Lazy worker?", dest = "lazyworker")

    elif msg == "tellcharob":
        inf.add_msg("We make it with my secret family recipe.")
        inf.add_link("It tastes great.", dest = "tastesgreat")
        inf.add_link("It stinks.", dest = "tastesbad")

    elif msg == "lazyworker":
        inf.add_msg("I suspect that it's that no good Steve Bruck.")

    elif msg == "tastesgreat":
        inf.add_msg("I'm glad you like it!")

    elif msg == "tastesbad":
        inf.add_msg("If you don't like it, go drink some slime!")

main()
inf.finish()

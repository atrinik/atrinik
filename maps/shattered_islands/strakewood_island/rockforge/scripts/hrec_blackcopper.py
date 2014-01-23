## @file
## Script for Hrec Blackcopper in Rockforge.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("Tread carefully, please, dear visitor! This farm is very important to us. Since Rockforge is a fortress, we usually do not go outside to collect timber, so we grow various plants here, inside the fortress.")
        inf.add_link("Don't plants need sunlight?", dest = "sunlight")

    elif msg == "sunlight":
        inf.add_msg("Yes, indeed. Just look up! It was quite amazing actually. We found this miles-long opening into the sky while digging tunnels. We reckon it is still part of the Giant Mountains, since it goes so high up! Anyway, we thought it may be good enough to grow plants, and it works!")
        inf.add_link("Those trees are somewhat small.", dest = "small")

    elif msg == "small":
        inf.add_msg("Yes, yes. That is because we are still underground, and even the opening above us doesn't provide enough sunlight for them to grow as tall as trees on the surface. Still, it's good enough for our needs.")

main()
inf.finish()

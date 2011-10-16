from Interface import Interface

inf = Interface(activator, me)

def main():
	if msg == "hello":
		inf.add_msg("Greetings {}. I am {}, the head researcher.".format(activator.name, me.name))
		inf.add_msg("Most assistants are with Albar in the eastern wing doing experiments on the Great Blue Crystal shard. I am currently researching the cave south of the town.")
		inf.add_link("Tell me more about the cave.", dest = "tellmore")

	elif msg == "tellmore":
		inf.add_msg("The cave is said to open each year when the Season of the Blizzard starts, and closes again when the season ends. The rumor is that some creature called Hierro lurks inside, but that's probably just a myth.")

main()
inf.finish()

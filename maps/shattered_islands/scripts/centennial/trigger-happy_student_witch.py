## @file
## Implements the trigger-happy student witch's responses.

from Interface import Interface

msg = msg.lower()

inf = Interface(activator, me)

def main():
	options = GetOptions()
	if options == "classTime":
		if msg == "hello":
			inf.add_msg("I wonder when I learn how to cast Master Spark.")
			inf.add_link("What's a Master Spark?", dest = "master spark")
		elif msg == "master spark":
			inf.add_msg("Master Spark, that's Marisa's favorite spell!")
			inf.add_link("Who is Marisa?", dest = "marisa")
		elif msg == "marisa" or msg == "kirisame" or msg == "marisa kirisame":
			inf.add_msg("She's the best, ze.")
	elif options == "clubTime":
		if msg == "hello":
			inf.add_msg("Hmm, still haven't learned how to cast Master Spark.  Maybe I can figure it out if I read some more of this story.")
			inf.add_link("What's a Master Spark?", dest = "master spark")
			inf.add_link("Which story?", dest = "touhou")
		elif msg == "master spark":
			inf.add_msg("Master Spark, that's Marisa's favorite spell!")
			inf.add_link("Who is Marisa?", dest = "marisa")
		elif msg == "marisa" or msg == "kirisame" or msg == "marisa kirisame":
			inf.add_msg("She's the best, ze.")
		elif msg == "marisa" or msg == "kirisame" or msg == "marisa kirisame":
			inf.add_msg("That's \"Eastern Project,\" ze.")

main()
inf.finish()

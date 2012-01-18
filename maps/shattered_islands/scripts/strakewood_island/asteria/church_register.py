## @file
## Script for Cathedral Registration attendants in the Catherdral of Holy Wisdom.

from Atrinik import Gender
from Interface import Interface

inf = Interface(activator, me)

def main():
	if msg == "hello":
		inf.add_msg("Good day.  Would you like to go in?")
		inf.add_link("Yes.", dest = "entry")
		inf.add_link("Never mind.  Bye.", action = "close")

	elif msg == "entry":
		if activator.GetGod() == "Tabernacle":
			if activator.GetGender() == Gender.MALE:
				T = "brother"
			elif activator.GetGender() == Gender.FEMALE:
				T = "sister"
			else:
				T = "err... friend"
			inf.add_msg("Go right on in {}, and may the blessings of the Tabernacle be with you.".format(T))
		elif activator.GetGod() == "Rashindel":
			inf.add_msg("What do you want here, devil-worshipper?  You may go in, but only so that the priests may soon drive that demonic drivel out of your head.")
		else:
			inf.add_msg("I see you are not of our faith.  Enter in peace and behold the great and powerful majesty of the Tabernacle.")

main()
inf.finish()

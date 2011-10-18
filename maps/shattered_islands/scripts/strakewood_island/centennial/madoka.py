## @file
## Implements Madoka, the instructor's dialog.

from Interface import Interface

msg = msg.lower()

inf = Interface(activator, me)

def main():
	if msg == "hello":
		inf.add_msg("*sigh* I wish they'd stop talking about those novels all day.")
	elif msg == "patchouli" or msg == "patchouli knowledge":
		inf.add_msg("Well, they say Patchouli is a gateway herb, or character as the case may be...  *chuckle*")
	elif msg == "puella magi madoka magica" or msg == "puella":
		inf.add_msg("Yeah, everyone in town is named from one novel or other.  The town is weird like that.")
	elif msg == "contract":
		inf.add_msg("What?  Are you trying to pretend to be Kyubey or something?")
		inf.add_link("Where is Kyubey?", dest = "kyubey")
	elif msg == "kyubey":
		inf.add_msg("No, we don't have a Kyubey...")

main()
inf.finish()

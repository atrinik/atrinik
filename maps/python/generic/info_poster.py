## @file
## Script implementing Information Posters.

from Atrinik import *
from PostOffice import *
from Guild import *

activator = WhoIsActivator()

def main():
	# Check which guild the player is member of.
	g = guild.pl_get_guild(activator.name)

	# Change guildname now.
	if g and g[2]:
		guild.set(g[0])

	# Messages of interest.
	messages = []
	# Bank balance.
	pinfo = activator.GetPlayerInfo("BANK_GENERAL")

	if pinfo and pinfo.value:
		messages.append("You have {} stored in bank.".format(CostString(pinfo.value)))

	# Any items in post office?
	if post.get_items():
		messages.append("There are items waiting for you in the post office.")

	# Are we in a guild and are we an admin of the guild?
	if g and g[2] and guild.member_is_admin(activator.name):
		count = 0

		# Count members awaiting approval.
		for member in guild.get_members():
			if not guild.member_approved(member):
				count += 1

		if count:
			messages.append("There are {} member(s) awaiting approval in your guild.".format(count))

	if messages:
		activator.Write("\n".join(messages), COLOR_WHITE)
	else:
		activator.Write("There is currently nothing of interest.", COLOR_WHITE)

post = PostOffice(activator.name)
guild = Guild(None)

try:
	main()
finally:
	guild.exit()
	post.db.close()
	SetReturnValue(1)

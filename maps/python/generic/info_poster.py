## @file
## Script implementing Information Posters.

from Atrinik import *
from PostOffice import *
from Guild import *

activator = WhoIsActivator()

post = PostOffice(activator.name)
guild = Guild(None)

def main():
	# Check which guild the player is member of.
	guildname = guild.is_in_guild(activator.name)

	# Change guildname now.
	if guildname:
		guild.guildname = guildname

	# Messages of interest.
	messages = []

	# Bank balance.
	pinfo = activator.GetPlayerInfo("BANK_GENERAL")

	if pinfo and pinfo.value:
		messages.append("You have {0} stored in bank.".format(CostString(pinfo.value)))

	# Any items in post office?
	if post.get_items():
		messages.append("There are items waiting for you in the post office.")

	# Are we in a guild and are we an admin of the guild?
	if guildname and guild.is_administrator(activator.name):
		count = 0

		# Count members awaiting approval.
		for member in guild.guilddb[guild.guildname]["members"]:
			if not guild.is_approved(member):
				count += 1

		if count:
			messages.append("There are {0} member(s) awaiting approval in your guild.".format(count))

	if messages:
		activator.Write("\n".join(messages), COLOR_WHITE)
	else:
		activator.Write("There is currently nothing of interest.", COLOR_WHITE)

try:
	main()
finally:
	guild.guilddb.close()
	post.db.close()
	SetReturnValue(1)

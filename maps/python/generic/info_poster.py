## @file
## Script implementing Information Posters.

from PostOffice import PostOffice
from Guild import Guild

post = PostOffice(activator.name)
guild = Guild(None)

def main():
	# Messages of interest.
	messages = []

	# Bank balance.
	balance = activator.Controller().BankBalance()

	if balance:
		messages.append("You have {} stored in bank.".format(CostString(balance)))

	# Any items in post office?
	if post.get_items():
		messages.append("There are items waiting for you in the post office.")

	# Check which guild the player is member of.
	g = guild.pl_get_guild(activator.name)

	# Change guildname now.
	if g and g[2]:
		guild.set(g[0])

		# Are we in a guild and are we an admin of the guild?
		if guild.member_is_admin(activator.name):
			count = 0

			# Count members awaiting approval.
			for member in guild.get_members():
				if not guild.member_approved(member):
					count += 1

			if count:
				messages.append("There are {} member(s) awaiting approval in your guild.".format(count))

	if messages:
		pl.DrawInfo("\n".join(messages), COLOR_WHITE)
	else:
		pl.DrawInfo("There is currently nothing of interest.", COLOR_WHITE)

try:
	main()
finally:
	post.db.close()
	SetReturnValue(1)

## @file
## Generic script for getting to the map with guild Oracle.
##
## Only allows entrance to guild administrators, DMs are an exception.

from Guild import Guild

guild = Guild(GetOptions())

def main():
	if not guild.member_is_admin(activator.name) and not activator.f_wiz:
		activator.Write("Entry forbidden. Only guild administrators are permitted.", COLOR_RED)
		pos = guild.get(guild.oracle_pos)

		if pos:
			activator.SetPosition(pos[0], pos[1])

		SetReturnValue(1)

main()

## @file
## Script used by apartment teleporters purchased in Brynknot.

def main():
	apartment = apartments_info[GetOptions()]
	pinfo = activator.FindObject(archname = "player_info", name = apartment["tag"])

	if not pinfo:
		activator.TeleportTo("/emergency", 0, 0)
	else:
		activator.TeleportTo(pinfo.race, pinfo.last_sp, pinfo.last_grace)

exec(open(CreatePathname("/python/generic/apartments.py")).read())
main()
SetReturnValue(1)

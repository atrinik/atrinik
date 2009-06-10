import Atrinik
import string

sglow_app_tag = "SGLOW_APP_INFO"
appid_cheap = "cheap"
appid_normal = "normal"
appid_expensive = "expensive"
appid_luxurious = "luxurious"
appid_special = "special"

me = Atrinik.WhoAmI()
ac = Atrinik.WhoIsActivator()

# search for the right player info
pinfo = ac.GetPlayerInfo(sglow_app_tag)
#no apartment - teleport him back 
if pinfo == None:
	me.SayTo(ac, "You don't own an apartment here!"); 
	ac.Write("A strong force teleports you away.", 0)
	ac.SetPosition(9, 18)
else:
	# thats the apartment tag
	pinfo_appid = pinfo.slaying
	if pinfo_appid == appid_cheap:
		ac.TeleportTo("/stoneglow/appartment_1", 1, 2, 1)
	elif pinfo_appid == appid_normal:
		ac.TeleportTo("/stoneglow/appartment_2", 1, 2, 1)
	elif pinfo_appid == appid_expensive:
		ac.TeleportTo("/stoneglow/appartment_3", 1, 2, 1)
	elif pinfo_appid == appid_luxurious:
		ac.TeleportTo("/stoneglow/appartment_4", 2, 1, 1)
	elif pinfo_appid == appid_special:
		ac.TeleportTo("/stoneglow/appartment_5", 2, 1, 1)
	else:
		me.SayTo(ac, "Wrong apartment ID?!"); 
		ac.Write("A strong force teleporting you backwards.", 0)
		ac.SetPosition(7, 20)

## @file
## Quests on Strakewood Island given by the quest giver in Brynknot.

## All the mini quests for Strakewood Island.
##
## The player will get the quest in the order this dictionary is. If the
## quest level is too high, they won't be able to get the quest.
quest_items = {
	"quest_1":
	{
		"level": 3,
		"messages":
		{
			"not_started": "Citizens have recently started complaining about ogres living in the ruins North of Brynknot. There have been feeble attacks on us by them. I think you should be able to take care of this for us.",
			"reward": "You receive five scrolls of minor healing (level 5).",
			"not_finished": "You still need to kill {0} ogre{1}.\nRemember, they live in the ruins North of Brynknot.",
			"kill_suffix_pl": "s",
			"kill_suffix": "",
			"accepted": "\nKill at least {0} ogres in the ruins North of Brynknot.",
		},
		"info":
		{
			"quest_name": "Ogres near Brynknot",
			"type": 1,
			"kills": 5,
			"message": "Kill 5 ogres in ruins North of Brynknot and return to Derowyn in Brynknot.",
		}
	},
	"quest_2":
	{
		"level": 5,
		"messages":
		{
			"not_started": "The treants and quickwoods East of Brynknot have always been peaceful with us. However, recently they have started attacking us the second they see us. I think you could help us out here.",
			"reward": "You receive a wand of firestorm (level 8).",
			"not_finished": "You still need to kill {0} evil treant{1}.\nThey live East of Brynknot.",
			"kill_suffix_pl": "s",
			"kill_suffix": "",
			"accepted": "\nKill at least {0} evil treants or quickwoods East of Brynknot.",
		},
		"info":
		{
			"quest_name": "Evil Trees at Brynknot",
			"type": 1,
			"kills": 6,
			"message": "Kill 6 evil treants or quickwoods East of Brynknot and return to Derowyn in Brynknot.",
		}
	},
	"quest_3":
	{
		"level": 6,
		"messages":
		{
			"not_started": "Recently, there have been sightings of skeletons coming back to life near the Forgotten Graveyard, located west of Aris. I think you should go check it out.",
			"reward": "You receive seven scrolls of recharge (level 10).",
			"not_finished": "You still need to kill {0} skeleton{1} from the Forgotten Graveyard North of Brynknot.\nRemember, they only appear at nighttime.",
			"kill_suffix_pl": "s",
			"kill_suffix": "",
			"accepted": "\nKill at least {0} skeletons at Forgotten Graveyard west of Aris.\nRemember, they only appear at nighttime.",
		},
		"info":
		{
			"quest_name": "Forgotten Graveyard at Strakewood Island",
			"type": 1,
			"kills": 10,
			"message": "Kill 10 skeletons at Forgotten Graveyard west of Aris and return to Derowyn in Brynknot. Remember, skeletons there only appear at nighttime.",
		}
	},
	"quest_4":
	{
		"level": 7,
		"messages":
		{
			"not_started": "For some reason, this year has been way too good for giant wasps, East of Brynknot. I trust you know what to do.",
			"reward": "You receive 7 silver coins.",
			"not_finished": "You still need to kill {0} giant wasp{1} East of Brynknot.\nRemember, they only appear at daytime.",
			"kill_suffix_pl": "s",
			"kill_suffix": "",
			"accepted": "\nKill at least {0} giant wasps East of Brynknot.\nRemember, they only appear at daytime.",
		},
		"info":
		{
			"quest_name": "Giant Wasps at Strakewood Island",
			"type": 1,
			"kills": 8,
			"message": "Kill 8 giant wasps East of Brynknot and return to Derowyn in Brynknot. Remember, giant wasps there only appear at daytime.",
		}
	},
	"quest_5":
	{
		"level": 10,
		"messages":
		{
			"not_started": "In the past few days, we've been having trouble with the wyverns living in cave Southeast of Brynknot. You seem strong enough, so I think you should be able to help us out.",
			"reward": "You receive amulet (ac+1).",
			"not_finished": "You still need to kill {0} wyvern{1} from the cave Southeast of Brynknot.",
			"kill_suffix_pl": "s",
			"kill_suffix": "",
			"accepted": "\nKill at least {0} wyverns in cave Southeast of Brynknot.",
		},
		"info":
		{
			"quest_name": "Wyverns at Strakewood Island",
			"type": 1,
			"kills": 7,
			"message": "Kill 7 wyverns in cave Southeast of Brynknot and return to Derowyn in Brynknot.",
		}
	},
	"quest_6":
	{
		"level": 12,
		"messages":
		{
			"not_started": "There have been rumors of ice golems living in an old outpost North of Aris in the Giant Mountains. I think you know what to do.",
			"reward": "You receive 10 silver coins.",
			"not_finished": "You still need to kill {0} ice golem{1} in the old outpost.",
			"kill_suffix_pl": "s",
			"kill_suffix": "",
			"accepted": "\nKill at least {0} ice golems in old outpost north of Aris in the Giant Mountains.",
		},
		"info":
		{
			"quest_name": "Ice Golems in Old Outpost",
			"type": 1,
			"kills": 10,
			"message": "Kill 10 ice golems in old outpost north of Aris in the Giant Mountains and return to Derowyn in Brynknot.",
		}
	},
	"quest_7":
	{
		"level": 13,
		"messages":
		{
			"not_started": "There have been rumors of lava golems living in an old outpost north of Aris in the Giant Mountains. I think you know what to do.",
			"reward": "You receive 15 silver coins.",
			"not_finished": "You still need to kill {0} lava golem{1} in the old outpost.",
			"kill_suffix_pl": "s",
			"kill_suffix": "",
			"accepted": "\nKill at least {0} lava golems in old outpost north of Aris in the Giant Mountains.",
		},
		"info":
		{
			"quest_name": "Lava Golems in Old Outpost",
			"type": 1,
			"kills": 10,
			"message": "Kill 10 lava golems in old outpost north of Aris in the Giant Mountains and return to Derowyn in Brynknot.",
		}
	},
}

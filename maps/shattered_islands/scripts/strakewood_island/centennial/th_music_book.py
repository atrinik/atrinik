## @file
## Script for the Music Book in the Centennial Library

from Interface import Interface
from Packet import MapStats

inf = Interface(activator, me)

def main():
	inf.set_icon("tome_glitter.101")
	inf.set_title("Eastern Project Music Book")

	if GetEventNumber() == EVENT_SAY:
		inf.dialog_close()
		MapStats(activator.Controller(), music = me.map.bg_music if msg == "none" else msg)
	else:
		inf.add_msg("This book seems strange.  You think the text is glowing for certain words...", COLOR_YELLOW)
		inf.add_msg("Eastern Project Music Book - Select a theme:")
		inf.add_link("BAD Apple!! (Elly, book 4)", dest = "th_18.mid")
		inf.add_link("Scarlet Beyond a Crimson Dream (book 6)", dest = "th_31.mid")
		inf.add_link("Apparitions Stalk the Night (Rumia, book 6 stg 1)", dest = "th_06.mid")
		inf.add_link("Lunate Elf (Daiyousei, book 6 stg 2)", dest = "th_14.mid")
		inf.add_link("Beloved Tomboyish Girl (Cirno, book 6 stg 2)", dest = "th_02.mid")
		inf.add_link("Shanghai Teahouse ~ Chinese Tea (book 6 stg 3)", dest = "th_28.mid")
		inf.add_link("Shanghai Alice of Meiji 17 (Meirin, book 6 stg 3)", dest = "th_03.mid")
		inf.add_link("Lunar Clock ~ Luna Dial (Sakuya, book 6 stg 5)", dest = "th_17.mid")
		inf.add_link("The Young Descendant of Tepes (Remilia, book 6 stg END)", dest = "th_22.mid")
		inf.add_link("Septette for the Dead Princess (Remilia, book 6 stg END)", dest = "th_13.mid")
		inf.add_link("Centennial Festival for Magical Girls (Patchouli, book 6 stg EX)", dest = "th_25.mid")
		inf.add_link("U.N. Owen was Her? (Flandre, book 6 stg EX)", dest = "th_10.mid")
		inf.add_link("Mystic Dream ~ Snow or Cherry Petal (book 7)", dest = "th_32.mid")
		inf.add_link("Crystallized Silver (Letty, book 7 stg 1)", dest = "th_30.mid")
		inf.add_link("Doll Judgment ~ The Girl Who Played With Peoples' Shapes (Alice, book 7 stg 3)", dest = "th_26.mid")
		inf.add_link("The Capital City of Flowers in the Sky (Lily White, book 7 stg 4)", dest = "th_21.mid")
		inf.add_link("Phantom Ensemble (Prismriver Sisters, book 7 stg 4)", dest = "th_29.mid")
		inf.add_link("A Maiden's Illusory Funeral ~ Necrofantasy (Ran, book 7 stg EX)", dest = "th_16.mid")
		inf.add_link("Necrofantasia (Yukari, book 7 stg PHANTASM)", dest = "th_12.mid")
		inf.add_link("Broken Moon (Suika, book 7.5 stg ENDS)", dest = "th_11.mid")
		inf.add_link("Deaf to All but the Song (Mystia, book 8 stg 2)", dest = "th_08.mid")
		inf.add_link("Maiden's Capriccio (Reimu, book 8 stg 4)", dest = "th_04.mid")
		inf.add_link("Love Colored Master Spark (Marisa, book 8 stg 4)", dest = "th_05.mid")
		inf.add_link("The Dark Side of Fate (Hina Kagiyama, book 10 stg 2)", dest = "th_24.mid")
		inf.add_link("The Gensokyo the Gods Loved (Nitori, book 10 stg 3)", dest = "th_07.mid")
		inf.add_link("Faith is for the Transient People (Sanae, book 10 stg 5)", dest = "th_01.mid")
		inf.add_link("Native Faith (Suwako, book 10 stg EX)", dest = "th_33.mid")
		inf.add_link("Catastrophe in Bhava-Agra ~ Wonderful Heaven (Tenshi, book 10.5 stg END)", dest = "th_27.mid")
		inf.add_link("Satori Maiden ~ 3rd Eye (Satori, book 11 stg 4)", dest = "th_02.mid")
		inf.add_link("Corpse Voyage ~ Be of Good Cheer (Rin, book 11 stg 5)", dest = "th_15.mid")
		inf.add_link("Solar Sect of Mystic Wisdom (Utsuho, book 11 stg END)", dest = "th_19.mid")
		inf.add_link("At the End of Spring (book 12 stg 1)", dest = "th_34.mid")
		inf.add_link("Beware the Umbrella Left Forever (Kogasa, book 12 stg 2)", dest = "th_20.mid")
		inf.add_link("Year Round Absorbed Curiosity (Fairies, book 12.8 stg 2)", dest = "th_23.mid")
		inf.add_link("Stop Music", dest = "none")

main()
inf.finish()

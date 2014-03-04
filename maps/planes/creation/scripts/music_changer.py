## @file
## Helpful NPC to change background music of a map.

from Interface import Interface
from Packet import MapStats
import re

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("Welcome. I am the music changer -- just tell me which background music file to play and I will change this map to play it. You can find a list of media files <a=url:http://www.atrinik.org/page/client_media_files>here</a>. Use [green]none[/green] to stop any music on the map.")
        inf.set_text_input()
    else:
        inf.dialog_close()
        info = me.map.LocateBeacon("creation_music_changer").env
        info.slaying = "no_music" if msg == "none" else msg.strip()
        # Construct new map name from old one.
        info.race = re.sub(r"\([a-zA-Z0-9_\-\.]+\)", "({})".format(info.slaying), info.race)
        map_name = "[b]<o=0,0,0>{}</o>[/b]".format(info.race)

        for player in me.map.GetPlayers():
            if player.x >= info.x and player.x <= info.x + info.hp and player.y >= info.y and player.y <= info.y + info.sp:
                MapStats(player.Controller(), name = map_name, music = info.slaying)

main()
inf.finish()

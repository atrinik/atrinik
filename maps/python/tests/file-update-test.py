## @file
## Tests the ability to download PNGs from the web (or stored locally, it matter little),
## and send them in file update packet to the client for use as textures. This means it's
# possible to use them with the <icon> tag, for example.

import os
import zlib
from urllib.request import urlopen


def send_file_update(pl, url):
    basename = os.path.basename(url)
    f = urlopen(url)
    s = f.read()
    f.close()
    pl.SendPacket(2, "sIx", "textures/" + basename, len(s), zlib.compress(s))

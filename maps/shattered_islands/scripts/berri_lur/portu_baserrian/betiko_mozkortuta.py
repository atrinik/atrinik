## @file
## Script for Betiko Mozkortuta in Portu Baserrian tavern.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("I'll haff ano'er beer peesh.")
        inf.add_link("What a drunkard...", dest = "drunkard")

    elif msg == "drunkard":
        inf.add_msg("I'm noh dwunk! I jus' lika dwenk. Or two.")

main()
inf.send()

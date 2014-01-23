## @file
## Implements Daril, a wizard at the Wizards' Tower.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("Welcome to my shop. Here you can buy various scrolls to help you in your adventures.")
        inf.add_msg("Have you heard about the legendary smoking pipe? They say it's the best pipe for smoking pipeweed.")
        inf.add_link("Tell me more...", dest = "tellmore")

    elif msg == "tellmore":
        inf.add_msg("If you smoke some pipeweed using that smoking pipe, you will be granted confusion and weapon magic protection, or so the rumour says.")
        inf.add_msg("It has long been lost, however...")
        inf.add_link("Any ideas where it could be?", dest = "location")

    elif msg == "location":
        inf.add_msg("Well, some rumours say that the fierce dragon leader Scursaur has it in his possession.")
        inf.add_msg("Of course, that's just a silly rumour... a dragon with a smoking pipe...")
        inf.add_link("Tell me about Scursaur.", dest = "aboutleader")

    elif msg == "aboutleader":
        inf.add_msg("He is the leader of the dragons on the Dragons Island. Apparently it's possible to get to Dragons Island by completing the Underground Maze under Promethia Island.")
        inf.add_msg("I wouldn't go there if I were you, though. The maze is one thing, but those dragons were sealed on that island for a reason...")

main()
inf.finish()

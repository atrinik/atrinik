## @file
## Script for Evelina Thash in the Asteria Library.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if msg == "hello":
        inf.add_msg("Welcome to the Asterian Royal Library, funded by our gracious king. Be sure to ask if you have any questions.")
        inf.add_link("The king?", dest = "king")
        inf.add_link("I have some questions...", dest = "questions")

    elif msg == "king":
        inf.add_msg("Believe it or not, King Romulus is quite generous to the intellectual pursuits.")
        inf.add_link("I have some questions...", dest = "questions")

    elif msg == "questions":
        inf.add_msg("The public area is free for all, but exercise your manners, please. The private area is only open to a privileged few. If you're unable to find a certain copy, notify me and I'll be sure to find it, as I have ample experience in this matter. If ever this becomes an issue, you had better first speak with me. I have more contacts than ways to name them.")
        inf.add_msg("Be sure to ask if you need help with the categorization scheme.")
        inf.add_link("Tell me about the categorization scheme.", dest = "tellscheme")

    elif msg == "tellscheme":
        inf.add_msg("The categorization scheme is rather simple at the moment. F denotes a fiction shelf. NF denotes a non-fiction shelf. The shelf is also labeled by general topic.")

main()
inf.finish()

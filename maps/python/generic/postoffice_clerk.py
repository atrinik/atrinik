## @file
## Generic script for post office clerks.

from Interface import Interface
from PostOffice import PostOffice

inf = Interface(activator, me)
post = PostOffice(activator.name)

def main():
    if msg == "hello":
        inf.add_msg("Welcome to our Post Office. You can check if someone has sent you items, or send an item to someone.")
        inf.add_link("I'd like to see items I have been sent.", dest = "items")
        inf.add_link("I'd like to send an item.", dest = "send1")

    elif msg == "send1":
        marked = activator.Controller().FindMarkedObject()
        error = post.check_send(marked)

        if error:
            inf.add_msg(error)
            return

        inf.add_msg("It will cost you {} to send the '{}'.".format(CostString(post.get_price(marked)), marked.GetName()))
        inf.add_msg_icon(marked.face[0], marked.GetName())
        inf.add_msg("If you are pleased with that, please type in the player's name that you want to send this item to.")
        inf.set_text_input(prepend = "send2 ")

    elif msg.startswith("send2 "):
        marked = activator.Controller().FindMarkedObject()
        name = msg[6:].capitalize()
        error = post.check_send(marked, name)

        if error:
            inf.add_msg(error)
            return

        cost = post.get_price(marked)

        if activator.PayAmount(cost):
            inf.add_msg("You pay {}.".format(CostString(cost)), COLOR_YELLOW)
            post.send_item(marked, name)
            inf.add_msg("The '{}' has been sent to {} successfully.".format(marked.GetName(), name))
            marked.Destroy()
        else:
            inf.add_msg("You don't have enough money.")

    elif msg == "items":
        items = post.get_items()

        if items:
            inf.add_msg("The following items have been sent to you:\n", newline = False)

            for i, item in enumerate(items):
                inf.add_msg("\n#{0}: {1} ({2}) ([a=:withdraw {0}]withdraw[/a], [a=:delete {0}]delete[/a])".format(i + 1, item["name"], item["from"]), newline = False)

            inf.add_link("I'd like to withdraw all.", dest = "withdraw 0")
        else:
            inf.add_msg("There is no mail for you right now.")

    elif msg.startswith("withdraw "):
        try:
            i = int(msg[9:])
        except:
            return

        inf.add_msg("\n".join(post.withdraw(activator, i)))

    elif msg.startswith("delete "):
        try:
            i = int(msg[7:])
        except:
            return

        inf.add_msg(post.delete(i))

try:
    main()
    inf.finish()
finally:
    post.db.close()

## @file
## Common functions used by the Auction House.

from Atrinik import *

## Possible coin archetypes, from most valued coin to least.
coin_archetypes = ["mitcoin", "goldcoin", "silvercoin", "coppercoin"]
## Coin names, most valued coin to least.
coin_names = ["mithril", "gold", "silver", "copper"]
## Coin values, most valued coin to least.
coin_values = [10000000, 10000, 100, 1]
## Maximum value of all items in a stack.
MAX_VALUE = coin_values[0] * 10
## Maximum number of items a player is allowed to sell at once.
MAX_ITEMS = 50
## Number of items per page.
PER_PAGE = 15

## Attempts to transform a string into integer cost, in the same syntax
## banks accept, eg, "10g 1 mithril 50silver".
## @param s String to transform.
## @return Amount of money as integer.
def string_to_cost(s):
    import re
    total = 0

    # Find all occurrences like "10 silver" or "1mith" in the string.
    for (val, coin) in re.findall(r"(\d+)\s?([a-z]+)", s, re.I):
        val = int(val)

        # Try to parse the coin name.
        for (i, coin_name) in enumerate(coin_names):
            if coin_name.startswith(coin):
                total += val * coin_values[i]
                break

    return total

## Transforms an amount of money as integer into a list of coins amount,
## from highest to lowest. For example, a 'total' of 150 would become a
## list containing: 0, 0, 1, 50
## @param total The integer total
## @return List containing coin amounts calculated from the total.
def cost_to_coins(total):
    l = [0] * len(coin_values)

    for (i, value) in enumerate(coin_values):
        if not total:
            break

        if total >= value:
            sub = total // value
            l[i] = sub
            total -= sub * value

    return l

## Create a container containing specified amount of money.
## @param value Amount of money the container will contain. cost_to_coins()
## is used to find out what coins to put in.
## @param msg Message the container will have.
## @return The created container.
def create_money_container(value, msg):
    cont = CreateObject("pouch")
    cont.name = "Auction House " + cont.name
    cont.value = 0
    cont.f_identified = True
    cont.f_is_named = True
    cont.f_inv_locked = True
    cont.msg = msg

    for (i, amount) in enumerate(cost_to_coins(value)):
        if amount:
            cont.CreateObject(coin_archetypes[i], nrof = amount)

    return cont

## Clears custom values of the specified object used for items inside the
## Auction House.
## @param obj The object that will have the custom values cleared.
def clear_custom_values(obj):
    obj.WriteKey("auction_house_seller")
    obj.WriteKey("auction_house_value")
    obj.WriteKey("auction_house_id")

## Buy an item from the Auction House.
## @param activator Who is buying the item.
## @param obj The item to buy.
## @param nrof Number of items to buy.
## @param seller The item seller.
## @return String explaining what happened (item was withdrawn/bought/etc).
def item_buy(activator, obj, nrof, seller):
    # Get the object's name with the correct nrof.
    old_nrof = obj.nrof
    obj.nrof = nrof
    obj_name = obj.GetName()
    obj.nrof = old_nrof

    ## Inserts the bought item inside the player, splitting it if necessary.
    def item_insert():
        # Different nrof, so split the object.
        if obj.nrof > 1 and obj.nrof != nrof:
            new = obj.Clone()
            new.nrof = nrof
            obj.nrof -= nrof
            clear_custom_values(new)
            new.InsertInto(activator)
        else:
            clear_custom_values(obj)
            obj.InsertInto(activator)

    # Seller is the same as the buyer, withdraw the item.
    if seller == activator.name:
        item_insert()
        return "You have withdrawn the {}.".format(obj_name)

    # Calculate the cost.
    cost = int(obj.ReadKey("auction_house_value")) * max(1, nrof)

    try:
        if not activator.PayAmount(cost):
            return "You lack {} to buy {}.".format(CostString(cost - activator.GetMoney()), obj_name)
    except OverflowError:
        return "Overflow error encountered; could not buy {}.\n\nTry buying a smaller quantity.".format(obj_name)

    import PostOffice

    # Create the container with the money.
    cont = create_money_container(cost, "What: {}\nPrice: {}.".format(obj_name, CostString(cost)))

    # Send the container.
    try:
        post = PostOffice.PostOffice(activator.name)
        post.send_item(cont, seller, 1)
    finally:
        post.db.close()

    cont.Destroy()
    item_insert()
    return "You paid {} for {}.".format(CostString(cost), obj_name)

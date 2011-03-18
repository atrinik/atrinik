## @file
## Provides the PostOffice class used by post office clerks and mailbox
## scripts.

from Atrinik import *
import shelve

## The PostOffice class.
class PostOffice:
	## Database file.
	db_file = "../server/data/postoffice"
	## The database dictionary.
	db = {}
	## The name of the player we're managing.
	name = ""

	## The constructor.
	## @param name Name of the player we're managing.
	def __init__(self, name):
		self.db = shelve.open(self.db_file)
		self.name = name

		if not name in self.db:
			self.init(name)

	## Initialize an entry for a player in database.
	def init(self, name):
		self.db[name] = []

	## Send an item.
	## @param object What are we sending?
	## @param who Who is receiving the item?
	## @param accepted Is the item accepted?
	def send_item(self, object, who, accepted = 0):
		if not who in self.db:
			self.init(who)

		temp = self.db[who]
		temp.append(
		{
			"contents": object.Save(),
			"name": object.GetName(),
			"from": self.name,
			"accepted": accepted,
		})
		self.db[who] = temp

	## Check if an item in the post office can be accepted or declined.
	## @param id ID of the item in the list.
	## @return True if it can be accepted/declined, False otherwise.
	def can_accept_or_decline(self, id):
		return self.db[self.name][id - 1]["accepted"] == 0

	## Accept an item.
	## @param id ID of the item in the list.
	def accept_item(self, id):
		temp = self.db[self.name]
		temp[id - 1]["accepted"] = 1
		self.db[self.name] = temp

	## Decline an item, sending it back to the original sender. The
	## original sender will not be able to decline it.
	## @param id ID of the item in the list.
	def decline_item(self, id):
		temp = self.db[self.name]
		item = temp.pop(id - 1)
		self.db[self.name] = temp
		sender = item["from"]
		temp = self.db[sender]
		item["from"] = self.name
		item["accepted"] = 1
		temp.append(item)
		self.db[sender] = temp

	## Get the list of items for player we're managing.
	## @return The list of the items.
	def get_items(self):
		return self.db[self.name]

	## Check if the player we're managing has at least one accepted item.
	## @return True if there is at least one accepted item, False
	## otherwise.
	def has_accepted_item(self):
		for item in self.db[self.name]:
			if item["accepted"] == 1:
				return True

		return False

	## Remove an item from the player's list we're managing.
	## @param id ID of the item in the list.
	def remove_item(self, id):
		temp = self.db[self.name]
		del temp[id]
		self.db[self.name] = temp

	## Check if object can be sent.
	## @param object The object to check.
	## @return True if it can be sent, False otherwise.
	def can_be_sent(self, object):
		if object.f_startequip or object.f_no_drop or object.f_quest_item:
			return False

		return True

	## Calculate how much it costs to send an item using the post office.
	##
	## Current formula is:
	## <pre>5% of object's value (for each object in a stack), always at
	## least 20 (copper).</pre>
	## @param object Object we want to send.
	## @return The calculated price.
	def get_price(self, object):
		return int(max(object.GetCost(object, COST_TRUE) / 100 * 5, 20))

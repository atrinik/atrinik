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
		})
		self.db[who] = temp

	## Get the list of items for player we're managing.
	## @return The list of the items.
	def get_items(self):
		return self.db[self.name]

	## Remove an item from the player's list we're managing.
	## @param id ID of the item in the list.
	def remove_item(self, id):
		temp = self.db[self.name]
		del temp[id]
		self.db[self.name] = temp

	def _withdraw_one(self, item, activator, pl, msgs):
		tmp = LoadObject(item["contents"])

		if not pl.CanCarry(tmp):
			msgs.append("The '{}' from {} is too heavy for you to carry.".format(item["name"], item["from"]))
			return False

		tmp.InsertInto(activator)
		msgs.append("You receive '{}' from {}.".format(item["name"], item["from"]))
		return True

	def withdraw(self, activator, i):
		pl = activator.Controller()
		msgs = []

		if not i:
			temp = [item for item in self.db[self.name] if not self._withdraw_one(item, activator, pl, msgs)]
			self.db[self.name] = temp
		else:
			try:
				item = self.db[self.name][i - 1]
			except IndexError:
				return msgs

			if self._withdraw_one(item, activator, pl, msgs):
				temp = self.db[self.name]
				del temp[i - 1]
				self.db[self.name] = temp

		return msgs

	def delete(self, i):
		temp = self.db[self.name]

		try:
			ret = "You deleted the '{}' from {}.".format(temp[i - 1]["name"], temp[i - 1]["from"])
			del temp[i - 1]
		except IndexError:
			return ""

		self.db[self.name] = temp
		return ret

	## Calculate how much it costs to send an item using the post office.
	##
	## Current formula is:
	## <pre>5% of object's value (for each object in a stack), always at
	## least 20 (copper).</pre>
	## @param object Object we want to send.
	## @return The calculated price.
	def get_price(self, object):
		return int(max(object.GetCost(object, COST_TRUE) / 100 * 5, 20))

	## Check for common situations where activator cannot send marked item.
	## @param object The marked object.
	## @param player Who we're sending object to.
	def check_send(self, object, player = None):
		# No object?
		if not object:
			return "You need to mark the item you want to send."
		# Cannot send locked objects...
		elif object.f_inv_locked:
			return "You must first unlock that item."
		# Nor applied ones
		elif object.f_applied:
			return "You must first unapply that item."
		# Don't allow sending containers with items inside it.
		elif object.type == Type.CONTAINER and object.inv:
			return "Due to heightened security levels all items must be removed from containers and sent separately."
		# Check if the item can be sent.
		elif object.f_startequip or object.f_no_drop or object.f_quest_item:
			return "You cannot send that item."
		elif object.quickslot:
			return "You must first remove that item from your quickslots."
		elif object.type == Type.MONEY:
			return "I'm terribly sorry, but we do not allow money to be sent."
		elif player:
			# Sending to ourselves?
			if player == self.name:
				return "You cannot send an item to yourself."
			# The player doesn't exist?
			elif not PlayerExists(player):
				return "That player doesn't exist."

		return None

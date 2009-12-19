## @file
## Implements the News class.

from Atrinik import *
import shelve
from datetime import datetime

## The News class.
class News:
	## Database file.
	db_file = "../server/data/news"
	## The database dictionary.
	db = {}
	## The location name we're managing.
	location = ""

	## The constructor.
	## @param location Location name we're managing.
	def __init__(self, location):
		self.db = shelve.open(self.db_file)
		self.location = location

		if location != "" and not location in self.db:
			self.db[location] = []

	## Add a message.
	## @param message The message to add.
	def add_message(self, message):
		temp = self.db[self.location]
		temp.insert(0,
		{
			"message": message,
			"time": self.get_time()
		})
		self.db[self.location] = temp

	## Remove a message.
	## @param id ID of the message to remove.
	def remove_message(self, id):
		temp = self.db[self.location]
		del temp[id]
		self.db[self.location] = temp

	## Remove all messages.
	def remove_all_messages(self):
		self.db[self.location] = []

	## Get messages.
	def get_messages(self):
		return self.db[self.location]

	## Get time used for the messages.
	def get_time(self):
		return datetime.now().strftime("%a %d. %b")

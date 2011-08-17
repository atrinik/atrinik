## @file
## API for handling taverns.

from Atrinik import *

## Implements tavern bartenders.
class Bartender:
	## Maximum number of provisions.
	max_provisions = 100

	## Initialize the bartender class.
	## @param activator Script activator.
	## @param me Bartender.
	## @param event Event object.
	def __init__(self, activator, me, event):
		self.activator = activator
		self.me = me
		self.event = event

	## Acquires the provisions menu.
	## @return The provisions.
	def _get_provisions(self):
		for x in sorted(self.event.inv, key = lambda obj: obj.name):
			yield "<a>" + x.name + "</a> for " + CostString(x.value)

	## Greeting ('hi', 'hey', 'hello')
	def _chat_greeting(self):
		self.me.SayTo(self.activator, "\nWelcome to my tavern, dear customer! I am {}.\nI can offer you the following <a>provisions</a>:\n\n{}".format(self.me.name, "\n".join(self._get_provisions())))

	## 'provisions'
	def _chat_provisions(self, num_max, example):
		self.me.SayTo(self.activator, "\nYou can either buy the provisions one by one, or prefix the provision name with a number (maximum {}), for example:\n<a>{} {}</a>".format(num_max, example[0], example[1]))

	## Message when buying something.
	def _chat_buy(self, num, obj):
		self.me.SayTo(self.activator, "\nHere you go, {} {}, just as you ordered!\nPleasure doing business with you!".format(num, obj.name))

	## Check if the specified provision can be bought.
	## @param name The provision name to check.
	def _can_buy(self, name):
		return True

	## Triggered when one cannot buy a provision.
	## @param name Provision that was attempted to be bought.
	def _chat_cannot_buy(self, name):
		pass

	## Handle a message.
	## @param msg Message to handle.
	def handle_msg(self, msg):
		if msg == "hi" or msg == "hey" or msg == "hello":
			self._chat_greeting()

		# Display information about provisions.
		elif msg == "provisions":
			self._chat_provisions(self.max_provisions, (min(self.max_provisions, 10), self.event.inv.name))

		else:
			# Try to parse number of provisions to buy from the message.
			try:
				# Split the string and get the (possible) number.
				num_str = msg.split(" ", 1)[0]
				# Parse the number into integer, but add limit.
				num = max(1, min(int(num_str), self.max_provisions))
				# Adjust the message, removing the number.
				msg = msg[len(num_str):].strip()
			except:
				num = 1

			# Search for the provision name.
			for obj in self.event.inv:
				# Matched?
				if obj.name.lower() != msg:
					continue

				# Check if the provision can be bought.
				if not self._can_buy(obj.name.lower()):
					self._chat_cannot_buy(obj.name.lower())
				# Make sure the player can actually carry the provisions.
				elif not self.activator.Controller().CanCarry(obj.weight * num + obj.carrying):
					self.me.SayTo(self.activator, "\nYou can't carry {} {}...".format(num, obj.name))
				# Pay the amount.
				elif self.activator.PayAmount(obj.value * num):
					self.activator.Write("You pay {} to {}.".format(CostString(obj.value * num), self.me.name), COLOR_YELLOW)
					self._chat_buy(num, obj)

					# Clone the object.
					clone = obj.Clone()
					# Adjust the nrof.
					clone.nrof = num
					# Reset value to arch default so the provisions stack properly
					# with regular food drops.
					clone.value = clone.arch.clone.value
					# Insert the clone into the player.
					clone.InsertInto(self.activator)
				else:
					self.me.SayTo(self.activator, "\nSorry, you don't have enough money...")

				break

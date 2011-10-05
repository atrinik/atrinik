## @file
## Generic script for post office mailboxes.

from PostOffice import PostOffice

post = PostOffice(activator.name)

def main():
	msgs = post.withdraw(activator, 0)

	if msgs:
		activator.Write("\n".join(msgs))
	else:
		activator.Write("There are no items for you.")

try:
	main()
finally:
	post.db.close()
	SetReturnValue(1)

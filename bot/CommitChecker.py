## @file
## Commits checker.

import threading, re

try:
	from bzrlib.repository import Repository
	from bzrlib.branch import Branch
	enabled = True
except ImportError:
	print("WARNING: Could not import bzrlib, Bazaar-related operations will be disabled.")
	enabled = False

## The commits checker class.
class CommitChecker(threading.Thread):
	## Initialize the thread.
	## @param config ConfigParser instance.
	## @param db The script's general database.
	## @param db_lock The database's thread lock.
	## @param bots List of bots.
	## @param irc_instances List of IRC instances.
	def __init__(self, config, db, db_lock, bots, irc_instances):
		self._config = config
		self._db = db
		self._db_lock = db_lock
		self._bots = bots
		self._irc_instances = irc_instances
		threading.Thread.__init__(self)

	## Run the thread.
	def run(self):
		# Go through configured branches.
		for url in self._config.get("General", "branches").split(","):
			# Acquire DB lock.
			self._db_lock.acquire()

			# Initialize the branch data at this URL if needed.
			if not url in self._db["branches"]:
				self._db["branches"][url] = {
					"revno": 0,
				}

			self._db_lock.release()

			# Open the branch.
			branch = Branch.open(url)
			# Get the latest revision number.
			revno = branch.revno()

			# Acquire lock, store old revision number, and update with new one.
			self._db_lock.acquire()
			old_revno = self._db["branches"][url]["revno"]
			self._db["branches"][url]["revno"] = revno
			self._db_lock.release()

			# If the previous revision number was not 0 and it is different from
			# the latest revision number, it's time to do notifications.
			if old_revno and old_revno != revno:
				# Get revision IDs.
				revids = [branch.get_rev_id(old_revno + i + 1) for i in range(revno - old_revno)]
				# Open the repository.
				repo = Repository.open(url)

				# Notify about all new revisions.
				for (i, revision) in enumerate(repo.get_revisions(revids)):
					# Get the revision's committer and remove the email part.
					committer = re.sub(" \<.+\>", "", revision.committer)
					# Construct the notify message.
					msg = "{0} committed r{1} to branch {2}: {3}".format(committer, old_revno + i + 1, revision.properties["branch-nick"], revision.message)

					for bot in self._bots:
						if bot.config.getboolean(bot.section, "dmsay_commits"):
							bot.command_queue_add("/dmsay {0}".format(msg), bot.config.getfloat(bot.section, "delay"))

					for irc in self._irc_instances:
						for channel in irc.channels:
							if irc.config.getboolean(irc.section, "notify_commits"):
								irc.command_queue_add("PRIVMSG {0} :{1}\r\n".format(channel, msg), irc.config.getfloat(irc.section, "delay"))

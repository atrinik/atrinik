#!/usr/bin/env python
# @file
# The Atrinik bot script.

import CommitChecker, time, threading, shelve
from misc import *
from Bot import *
from IRC import *
from howie import Howie

try:
	from ConfigParser import ConfigParser
# Python 3.x
except:
	from configparser import ConfigParser

## Create database lock.
db_lock = threading.RLock()
## Create the database.
db = shelve.open("bot.db", writeback = True)
bots = []
irc_instances = []

## The main function.
def main():
	# Create a new ConfigParser and read the config.
	config = ConfigParser()
	config.read(["config.cfg"])
	chatbot = Howie.Howie()

	# Check configuration sections for game and IRC bots.
	for section in config.sections():
		if section.startswith("Game"):
			bot = Bot((config.get(section, "host"), config.getint(section, "port"), config.get(section, "name"), config.get(section, "pswd")), bots, config, section)
			bot.howie = chatbot
			bots.append(bot)
		elif section.startswith("IRC"):
			irc_instances.append(IRC((config.get(section, "host"), config.getint(section, "port"), config.get(section, "name"), config.get(section, "pswd"), config.get(section, "channels").split(",")), config, section))

	# If commits checker is enabled, do some work.
	if CommitChecker.enabled:
		commits_check = time.time() + config.getfloat("General", "commits_check_delay")

		if not "branches" in db:
			db["branches"] = {}

	# The main infinite loop.
	while True:
		# No bots and no IRC connections left, bail out.
		if not bots and not irc_instances:
			print("No bots running left, bailing out.")
			break

		# Get the current time.
		t1 = time.time()

		# If commits checker is enabled and enough time has elapsed since
		# last time we checked for new commits, try to check for new commits
		# again.
		if CommitChecker.enabled and t1 >= commits_check:
			# Create the commits checker thread and start it.
			thread = CommitChecker.CommitChecker(config, db, db_lock, bots, irc_instances)
			thread.start()
			# Update time of next check.
			commits_check = t1 + config.getfloat("General", "commits_check_delay")

		# Handle all active connections.
		for connection in bots + irc_instances:
			if not connection.handle_reconnect():
				continue

			connection.handle_data()
			connection.handle_extra()

		# Get the current time again.
		t2 = time.time()
		# Figure out how long to sleep for.
		sleep_time = config.getfloat("General", "sleep_time") - (t2 - t1)

		# Sleep for so much time if possible (do not try sleeping for
		# negative amounts of time).
		if sleep_time > 0.0:
			time.sleep(sleep_time)

try:
	main()
finally:
	# Close all connections.
	for connection in bots + irc_instances:
		connection.close()

	# Close the database.
	db_lock.acquire()
	db.close()
	db_lock.release()

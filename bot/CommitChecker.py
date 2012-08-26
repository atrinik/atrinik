#*************************************************************************
#*            Atrinik, a Multiplayer Online Role Playing Game            *
#*                                                                       *
#*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
#*                                                                       *
#* Fork from Crossfire (Multiplayer game for X-windows).                 *
#*                                                                       *
#* This program is free software; you can redistribute it and/or modify  *
#* it under the terms of the GNU General Public License as published by  *
#* the Free Software Foundation; either version 2 of the License, or     *
#* (at your option) any later version.                                   *
#*                                                                       *
#* This program is distributed in the hope that it will be useful,       *
#* but WITHOUT ANY WARRANTY; without even the implied warranty of        *
#* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
#* GNU General Public License for more details.                          *
#*                                                                       *
#* You should have received a copy of the GNU General Public License     *
#* along with this program; if not, write to the Free Software           *
#* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
#*                                                                       *
#* The author can be reached at admin@atrinik.org                        *
#*************************************************************************

## @file
## Commits checker.

import threading, re
from xml.sax import saxutils

try:
	from bzrlib.repository import Repository
	from bzrlib.branch import Branch
	import bzrlib
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
	## @param cia CIA bot instance.
	def __init__(self, config, db, db_lock, bots, cia):
		self._config = config
		self._db = db
		self._db_lock = db_lock
		self._bots = bots
		self._cia = cia
		threading.Thread.__init__(self)

	def generate_cia_xml(self, branch, revno, revision, projectname):
		delta = branch.repository.get_revision_delta(revision.revision_id)
		authors = revision.get_apparent_authors()

		files = []
		[files.append(f) for (f,_,_) in delta.added]
		[files.append(f) for (f,_,_) in delta.removed]
		[files.append(f) for (_,f,_,_,_,_) in delta.renamed]
		[files.append(f) for (f,_,_,_,_) in delta.modified]

		authors_xml = "".join(["			<author>{0}</author>\n".format(saxutils.escape(re.sub(" \<.+\>", "", author))) for author in authors])

		return """
<message>
	<generator>
		<name>bzr</name>
		<version>{0}</version>
		<url>http://www.atrinik.org/</url>
	</generator>
	<source>
		<project>{1}</project>
		<module>{2}</module>
	</source>
	<timestamp>{3}</timestamp>
	<body>
		<commit>
			<revision>{4}</revision>
			<files>
				{5}
			</files>
			{6}<log>{7}</log>
		</commit>
	</body>
</message>""".format(bzrlib.version_string, projectname, branch.nick, int(revision.timestamp - revision.timezone), revno, "\n".join(["<file>{0}</file>".format(saxutils.escape(f)) for f in files]), authors_xml, saxutils.escape(revision.message))

	## Run the thread.
	def run(self):
		# Go through configured branches.
		for entry in self._config.get("General", "branches").split(","):
			(url, projectname) = entry.split(" ")

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

					if self._cia:
						self._cia.submit(self.generate_cia_xml(branch, old_revno + i + 1, revision, projectname))

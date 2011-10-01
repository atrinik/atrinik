import os, shutil

# Check whether the passed string is an integer.
# @param s String.
# @return True if s is an integer, False otherwise.
def isint(s):
	try:
		int(s)
		return True
	except ValueError:
		return False

# Get attribute ID in an object's list of attributes.
# @param arch Object.
# @param attr_search Attribute name we're searching for.
# @param new_default If set and attribute is not found, create
# the attribute with this value, and return the attribute's ID.
# @return Attribute ID if found, -1 otherwise.
def arch_get_attr_num(arch, attr_search, new_default = None):
	i = 0

	for [attr, val] in arch["attrs"]:
		if attr == attr_search:
			return i

		i += 1

	if new_default != None:
		arch["attrs"].append([attr_search, new_default])
		return i

	return -1

# Convenience function to get attribute's value by using
# arch_get_attr_num().
# @param arch Object.
# @param attr_search Attribute name we're searching for.
# @return Attribute value if found, None otherwise.
def arch_get_attr_val(arch, attr_search):
	i = arch_get_attr_num(arch, attr_search)

	if i != -1:
		return arch["attrs"][i][1]

	return None

# Data files traverser.
class Traverser:
	# Initializer.
	# @param path Path we're going through.
	def __init__(self, path):
		self.path = path

	# Get files from path.
	# @param path Path to get files from.
	# @return List of the files found.
	def get_files_path(self, path):
		items = os.listdir(path)
		files = []

		for item in items:
			file = path + "/" + item

			if os.path.isdir(file):
				files.extend(self.get_files_path(file))
			elif os.path.isfile(file):
				files.append(file)

		return files

	def get_files(self):
		return self.get_files_path(self.path + "/players") + self.get_files_path(self.path + "/unique-items")

# Map object loader.
class MapObjectParser:
	# Initializer.
	# @param fp File pointer.
	# @param upgrade_func Upgrade function to call on each object.
	def __init__(self, fp, upgrade_func = None):
		self.fp = fp
		self.in_msg = False
		self.msg_buf = ""
		# Map arches.
		self.arches = []
		self.upgrade_func = upgrade_func
		self.player = []

	# Check if the file pointer is a data file.
	# @return True if it is a data file, False otherwise.
	def is_data_file(self):
		s = self.fp.read(512)

		if not "\0" in s and (s[:5] == "arch " or s[:9] == "password "):
			self.fp.seek(0)
			return True

		return False

	# Set our file pointer.
	# @param new_fp File pointer to set.
	def set_fp(self, new_fp):
		self.fp = new_fp

	# Save an arch. This function is recursive.
	# @param arch Arch to save.
	def save_arch(self, arch):
		# Is this a player we're doing?
		if self.player:
			for line in self.player:
				self.fp.write(line)

			self.fp.write("endplst\n")
			self.player = []

		self.fp.write("arch {0}\n".format(arch["archname"]))

		# Save the attributes.
		for [attr, val] in arch["attrs"]:
			if attr == "msg":
				self.fp.write("msg\n{0}\nendmsg\n".format(val))
			else:
				self.fp.write("{0} {1}\n".format(attr, val))

		# Recursively save the inventory.
		if arch["inv"]:
			for arch_inv in arch["inv"]:
				self.save_arch(arch_inv)

		self.fp.write("end\n")

	# Save all the arches previously loaded.
	def save(self):
		if not self.arches:
			return

		for arch in self.arches:
			self.save_arch(arch)

	# Load all arches on map or in player's inventory.
	# @return The arches.
	def load(self):
		in_player = False

		for line in self.fp:
			if line[:9] == "password ":
				in_player = True
			elif line == "endplst\n":
				in_player = False
				continue

			if in_player:
				self.player.append(line)
			elif line[:5] == "arch ":
				arch = self.map_parse_rec(line[5:-1])

				if arch:
					self.arches.append(arch)

		return self.arches

	# Recursively parse objects on map.
	# @param archname Arch name we previously found.
	# @param env If True, we are inside another arch.
	# @return Archetype, complete with its inventory.
	def map_parse_rec(self, archname, env = False):
		archetype = {}
		# Store its name.
		archetype["archname"] = archname
		# Inventory.
		archetype["inv"] = []
		archetype["attrs"] = []

		if not env:
			archetype["x"] = 0
			archetype["y"] = 0

		for line in self.fp:
			# Another arch? That means it's inside the previous one.
			if line[:5] == "arch ":
				arch = self.map_parse_rec(line[5:-1], True)

				# Add it to the object's inventory.
				if arch:
					archetype["inv"].append(arch)
			elif line == "end\n":
				break
			# Parse attributes.
			else:
				parsed = self.parse(line)

				if parsed:
					archetype["attrs"].append(parsed)

		if self.upgrade_func:
			archetype = self.upgrade_func(archetype)

		return archetype

	# Parse attributes from a line.
	# @param line Line to parse from.
	# @return Attribute and value on success, None if there's nothing to parse.
	def parse(self, line):
		# Message start?
		if line == "msg\n":
			self.in_msg = True
			self.msg_buf = ""
		# End of message.
		elif line == "endmsg\n":
			self.in_msg = False
			return ["msg", self.msg_buf[:-1]]
		# We are in a message, store it in a buffer.
		elif self.in_msg:
			self.msg_buf += line
		# Not a message, so attribute/value combo.
		else:
			# Find space.
			space_pos = line.find(" ")
			# Our value.
			value = line[space_pos + 1:-1]

			if isint(value):
				value = int(value)

			attr = line[:space_pos]

			if space_pos == -1 and attr[:3] in ("Str", "Dex", "Con"):
				attr = attr[:3]
				value = int(value[3:])

			return [attr, value]

		return None

# The actual object upgrader.
class ObjectUpgrader:
	# Initialize.
	# @param files The files we're going to upgrade.
	# @param upgrade_func Function we'll call for each object.
	def __init__(self, files, upgrade_func = None):
		self.files = files
		self.upgrade_func = upgrade_func
		self.player_upgrade_func = None

	def set_player_upgrade_func(self, upgrade_func):
		self.player_upgrade_func = upgrade_func

	# Do the actual upgrading.
	def upgrade(self):
		for file in self.files:
			if not os.path.exists(file):
				continue

			fp = open(file, "r")
			# Load object parser.
			parser = MapObjectParser(fp, self.upgrade_func)

			# Ensure this is a data file.
			if not parser.is_data_file():
				continue

			# Parse the objects.
			arches = parser.load()
			fp.close()

			# If we found anything, save the objects.
			if arches:
				if parser.player and self.player_upgrade_func:
					(parser.player, arches) = self.player_upgrade_func(parser.player, arches)

					if not parser.player:
						shutil.rmtree(os.path.dirname(file))
						continue

				fp = open(file, "w")
				parser.set_fp(fp)
				parser.save()
				fp.close()

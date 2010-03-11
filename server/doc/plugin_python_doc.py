## This script will look in Python plugin source files for fields,
## flags and constants to transform to Doxygen documentation.

## Load definitions from a file.
## @param file File to load from.
## @param dict Dictionary to put flags etc into.
def load_defines(file, dict):
	num_mapflags = 0

	fp = open(file, "r")

	# Need to go through define.h to figure out the flags
	for line in fp:
		# A flag?
		if line[:13] == "#define FLAG_":
			# Get its name
			flag_name = line[8:line.find(" ", 8)]
			# And then it's integer value
			flag_id = int(line[8 + len(flag_name):].strip())
			dict[flag_id] = flag_name
		elif line[:17] == "#define MAP_FLAG_":
			# Only need the flag name
			flag_name = line[8:line.find(" ", 8)]
			dict[num_mapflags] = flag_name
			num_mapflags += 1

	fp.close()

## Transform fieldtype macro type to something that can be understood by
## scripters.
## @param fieldtype The field type, as found in the code.
## @return Easier-to-understand version of the field type.
def fieldtype_to_string(fieldtype):
	if fieldtype == "FIELDTYPE_CSTR" or fieldtype == "FIELDTYPE_SHSTR" or fieldtype == "FIELDTYPE_CARY":
		return "string"
	elif fieldtype == "FIELDTYPE_OBJECT" or fieldtype == "FIELDTYPE_OBJECTREF":
		return "object"
	elif fieldtype == "FIELDTYPE_UINT64" or fieldtype == "FIELDTYPE_SINT64" or fieldtype == "FIELDTYPE_UINT32" or fieldtype == "FIELDTYPE_SINT32" or fieldtype == "FIELDTYPE_UINT16" or fieldtype == "FIELDTYPE_SINT16" or fieldtype == "FIELDTYPE_UINT8" or fieldtype == "FIELDTYPE_SINT8":
		return "integer"
	elif fieldtype == "FIELDTYPE_FLOAT":
		return "float"
	elif fieldtype == "FIELDTYPE_MAP":
		return "map"
	elif fieldtype == "FIELDTYPE_REGION":
		return "region"

	return "unknown"

## Parse a single code file.
## @param name Type of the file we're parsing, something like 'object', 'map', etc.
## @param params Parameters from documentation dictionary.
def parse_code_file(name, params):
	in_fields = False
	in_flags = False
	in_constants = False
	num_flags = 0

	# Open the file
	fp = open(code_dir + "/" + params["file"], "r")

	for line in fp:
		if "fields" in params and line == params["fields"] + "\n":
			in_fields = True
			doc_fp.write("\n@page plugin_python_%(name)s_fields Python %(name)s fields\nList of the %(name)s fields and their meaning:\n" % {"name": name})
		elif line == "};\n":
			in_fields = False
			in_flags = False
			in_constants = False
		elif in_fields:
			# Get the text between '{' and '}'.
			parts = line[line.find("{") + 1:line.rfind("}")].split(",")

			if len(parts) < 4:
				continue

			# The field name (without double quotes)
			field_name = parts[0][1:-1]
			# The field type (ie, FIELDTYPE_MAP)
			field_type = parts[1].strip()
			# Get the first parameter of the 'offsetof' macro
			field_struct = parts[2].strip()[9:]
			# Now get the second parameter of the 'offsetof' macro
			field_struct_member = parts[3].strip()[:-1]

			# To fix references
			if field_struct_member[:6] == "stats.":
				field_struct_member = field_struct_member[6:]
				field_struct = "living"

			doc_fp.write("\n- <b>%s</b>: (%s) @copydoc %s::%s" % (field_name, fieldtype_to_string(field_type), field_struct, field_struct_member))
		elif "flags" in params and line == params["flags"] + "\n":
			in_flags = True
			doc_fp.write("\n@page plugin_python_%(name)s_flags Python %(name)s flags\nList of the %(name)s flags and their meaning:\n" % {"name": name})
		elif in_flags:
			if line == "{\n":
				continue

			# Split it into pieces, as one line can have multiple flags
			parts = line.split(",")

			for part in parts:
				part = part.strip()

				# 'NULL'? Then we don't want this...
				if part == "NULL":
					num_flags += 1
					continue
				elif part == "":
					continue

				# Get rid of double quotes
				part = part[1:-1]
				doc_fp.write("\n- <b>%s</b>: @copydoc %s" % (part, params["flags_collected"][num_flags]))
				num_flags += 1
		elif "constants" in params and line == params["constants"] + "\n":
			in_constants = True
			doc_fp.write("\n@page plugin_python_%(name)s_constants Python %(name)s constants\nList of the %(name)s constants and their meaning:\n" % {"name": name})
		elif in_constants:
			# For constants, we should always get 2 entries here
			parts = line.split(",")

			if len(parts) < 2:
				continue

			# Now get rid of '{' and whitespace
			constant_name = parts[0].strip()[1:]

			# 'NULL' marks the end of the constants
			if constant_name == "NULL":
				continue

			# Get rid of double quotes
			constant_name = constant_name[1:-1]
			# Get rid of whitespace and ending '}'
			constant_value = parts[1].strip()[:-1]

			doc_fp.write("\n- <b>%s</b>: " % constant_name)

			# If it's digit, there's not much else we can do to get documentation about it
			if constant_value.isdigit():
				doc_fp.write("%s" % constant_value)
			# A type, format it nicely and make a link.
			elif constant_name[:5] == "TYPE_":
				doc_fp.write("@ref %s \"%s\"" % (constant_value, constant_name[5:].replace("_", " ").title()))
			# Just do a @copydoc
			else:
				doc_fp.write("@copydoc %s" % constant_value)

	fp.close()

## The plugin's directory.
code_dir = "../src/plugins/plugin_python"

## Dictionary with all the information of what to turn into documentation.
documentation = {
	"object":
	{
		"file": "atrinik_object.c",
		"fields": "obj_fields_struct obj_fields[] =",
		"flags": "static char *flag_names[NUM_FLAGS + 1] =",
		"flags_collected": {},
		"constants": "static Atrinik_Constant object_constants[] =",
	},
	"map":
	{
		"file": "atrinik_map.c",
		"fields": "map_fields_struct map_fields[] =",
		"flags": "static char *mapflag_names[] =",
		"flags_collected": {},
		"constants": "static Atrinik_Constant map_constants[] =",
	},
	"party":
	{
		"file": "atrinik_party.c",
		"fields": "party_fields_struct party_fields[] =",
		"constants": "static Atrinik_Constant party_constants[] =",
	},
	"general":
	{
		"file": "plugin_python.c",
		"constants": "static Atrinik_Constant module_constants[] =",
	},
	"region":
	{
		"file": "atrinik_region.c",
		"fields": "region_fields_struct region_fields[] =",
	},
}

# Load some defines.
load_defines("../src/include/define.h", documentation["object"]["flags_collected"])
load_defines("../src/include/map.h", documentation["map"]["flags_collected"])

## The file that will receive the documentation output
doc_fp = open("plugin_python_doc.dox", "w")
doc_fp.write("/**\n")

# Now parse each file
for doc in documentation:
	parse_code_file(doc, documentation[doc])

## What to generate @see links for.
see_links = ["fields", "flags", "constants"]

## Generate @see links.
## @param name Type of the @see links, an entry in the documentation dictionary.
## @param extra Extra text to put before the automatically-generated ones.
def make_see_links(name, extra = []):
	what_to_see = extra

	for link in see_links:
		if link in documentation[name]:
			what_to_see.append("@ref plugin_python_%s_%s" % (name, link))

	if what_to_see:
		doc_fp.write("\n\n@see %s" % ", ".join(what_to_see))

# Python plugin introduction
doc_fp.write("\n@page page_plugin_python Python Plugin\n\n@section sec_plugin_python_introduction Introduction\n\nThe Python plugin is used in various important tasks thorough Atrinik maps. It plays an important role in making quests, events, shop NPCs and much much more.\n\nPython scripts in the game allow a lot greater flexibility than changing the server core code to implement a quest or a new type of NPC. It is not a fast way to do it, true, but flexible, because Python scripts can be changed on the fly without even restarting the server.")
# Section about the general plugin
doc_fp.write("\n\n@section sec_plugin_python_atrinik Atrinik Python plugin functions\n\nThe Atrinik Python plugin functions are used to make an interface with the Atrinik C server code, get script options, activator, etc.")
make_see_links("general", ["plugin_python_functions"])
# Section about maps
doc_fp.write("\n\n@section sec_plugin_python_atrinik_map Atrinik Map Python plugin functions\n\nThe Atrinik Map Python plugin functions allow you to access map related functions, the map structure fields, and so on.")
make_see_links("map", ["plugin_python_map_functions"])
# Section about objects
doc_fp.write("\n\n@section sec_plugin_python_atrinik_object Atrinik Object Python plugin functions\n\nThe Atrinik Object Python plugin functions allow you great flexibility in manipulating objects, players, and about everything related to object structure.")
make_see_links("object", ["plugin_python_object_functions"])
# Section about parties
doc_fp.write("\n\n@section sec_plugin_python_atrinik_party Atrinik Party Python plugin functions\n\nThe Atrinik Party Python plugin functions allow you to make interesting events or dungeons, where in order to participate, one must/mustn't be in a party.")
make_see_links("party", ["plugin_python_party_functions"])
# Section about regions
doc_fp.write("\n\n@section sec_plugin_python_atrinik_region Atrinik Region Python plugin functions\n\nProvides an interface to get information about map's region.")
make_see_links("region")

doc_fp.write("\n*/\n")
doc_fp.close()

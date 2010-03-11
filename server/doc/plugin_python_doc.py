# This script will look in Python plugin source files for fields,
# flags and constants to transform to Doxygen documentation.

# The plugin's directory.
code_dir = "../src/plugins/plugin_python"
# Where define.h is.
define_h = "../src/include/define.h"
# Where map.h is.
map_h = "../src/include/map.h"

# Transform fieldtype macro type to something that can be understood by
# scripters.
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

flags = {}
types = {}
in_types = False

fp = open(define_h, "r")

# Need to go through define.h to figure out the flags and types
for line in fp:
	# A flag?
	if line[:13] == "#define FLAG_":
		# Get its name
		flag_name = line[8:line.find(" ", 8)]
		# And then it's integer value
		flag_id = line[8 + len(flag_name):].strip()
		flags[flag_id] = flag_name
	# The start of type defines?
	elif line == " * @defgroup type_defines Type defines\n":
		in_types = True
	# Type defines ended
	elif line == "/*@}*/\n":
		in_types = False
	# We are in type defines, and this is starting with "#define "?
	elif in_types and line[:8] == "#define ":
		# Get its name
		type_name = line[8:line.find(" ", 8)]
		# And the integer value
		type_id = line[8 + len(type_name):].strip()
		types[type_id] = type_name

fp.close()

mapflags = {}
num_mapflags = 0

fp = open(map_h, "r")

# Now look for map flags; as the flags are bitmaps, we need to store
# a temporary integer variable above to know which one we're adding
# now.
for line in fp:
	if line[:17] == "#define MAP_FLAG_":
		# Only need the flag name
		flag_name = line[8:line.find(" ", 8)]
		mapflags[num_mapflags] = flag_name
		num_mapflags += 1

fp.close()

# The file that will receive the documentation output
doc_fp = open("plugin_python_doc.dox", "w")
doc_fp.write("/**\n")

# Generate documentation for objects
fp = open(code_dir + "/atrinik_object.c", "r")

in_fields = False
in_flags = False
in_constants = False
num_flags = 0

for line in fp:
	if line == "obj_fields_struct obj_fields[] =\n":
		in_fields = True
		doc_fp.write("\n@page plugin_python_object_fields Python object fields\nList of the object fields and their meaning:\n")
	elif line == "};\n":
		in_fields = False
		in_flags = False
		in_constants = False
	elif in_fields:
		parts = line.split(",")

		if len(parts) < 5:
			continue

		field_name = parts[0].strip()[2:-1]
		field_type = parts[1].strip()
		field_offset = parts[2].strip()[9:]
		field_offset2 = parts[3].strip()[:-1]

		if field_offset2[:6] == "stats.":
			field_offset2 = field_offset2[6:]
			field_offset = "living"

		doc_fp.write("\n- <b>%s</b>: (%s) @copydoc %s::%s" % (field_name, fieldtype_to_string(field_type), field_offset, field_offset2))
	elif line == "static char *flag_names[NUM_FLAGS + 1] =\n":
		in_flags = True
		doc_fp.write("\n@page plugin_python_object_flags Python object flags\nList of the object flags and their meaning:\n")
	elif in_flags:
		if line == "{\n":
			continue

		parts = line.split(",")

		for part in parts:
			part = part.strip()

			if part == "NULL":
				num_flags += 1
				continue
			elif part == "":
				continue

			part = part[1:-1]
			doc_fp.write("\n- <b>%s</b>: @copydoc %s" % (part, flags[str(num_flags)]))
			num_flags += 1
	elif line == "static Atrinik_Constant object_constants[] =\n":
		in_constants = True
		doc_fp.write("\n@page plugin_python_object_constants Python object constants\nList of the object constants and their meaning:\n")
	elif in_constants:
		parts = line.split(",")

		if len(parts) < 2:
			continue

		constant_name = parts[0].strip()[1:]

		if constant_name == "NULL":
			continue

		constant_name = constant_name[1:-1]
		constant_value = parts[1].strip()[:-1]

		doc_fp.write("\n- <b>%s</b>: " % constant_name)

		if constant_value.isdigit():
			doc_fp.write("%s" % constant_value)
		elif constant_name[:5] == "TYPE_":
			doc_fp.write("%s" % constant_name[5:].replace("_", " ").title())
		else:
			doc_fp.write("@copydoc %s" % constant_value)

fp.close()
# Documentation for map
fp = open(code_dir + "/atrinik_map.c", "r")

in_fields = False
in_flags = False
in_constants = False
num_flags = 0

for line in fp:
	if line == "map_fields_struct map_fields[] =\n":
		in_fields = True
		doc_fp.write("\n@page plugin_python_map_fields Python map fields\nList of the map fields and their meaning:\n")
	elif line == "};\n":
		in_fields = False
		in_flags = False
		in_constants = False
	elif in_fields:
		parts = line.split(",")

		if len(parts) < 4:
			continue

		field_name = parts[0].strip()[2:-1]
		field_type = parts[1].strip()
		field_offset = parts[2].strip()[9:]
		field_offset2 = parts[3].strip()[:-2]

		doc_fp.write("\n- <b>%s</b>: (%s) @copydoc %s::%s" % (field_name, fieldtype_to_string(field_type), field_offset, field_offset2))
	elif line == "static char *mapflag_names[] =\n":
		in_flags = True
		doc_fp.write("\n@page plugin_python_map_flags Python map flags\nList of the map flags and their meaning:\n")
	elif in_flags:
		if line == "{\n":
			continue

		parts = line.split(",")

		for part in parts:
			part = part.strip()

			if part == "NULL":
				num_flags += 1
				continue
			elif part == "":
				continue

			part = part[1:-1]
			doc_fp.write("\n- <b>%s</b>: @copydoc %s" % (part, mapflags[num_flags]))
			num_flags += 1
	elif line == "static Atrinik_Constant map_constants[] =\n":
		in_constants = True
		doc_fp.write("\n@page plugin_python_map_constants Python map constants\nList of the map constants and their meaning:\n")
	elif in_constants:
		parts = line.split(",")

		if len(parts) < 2:
			continue

		constant_name = parts[0].strip()[1:]

		if constant_name == "NULL":
			continue

		constant_name = constant_name[1:-1]
		constant_value = parts[1].strip()[:-1]

		doc_fp.write("\n- <b>%s</b>: " % constant_name)

		if constant_value.isdigit():
			doc_fp.write("%s" % constant_value)
		else:
			doc_fp.write("@copydoc %s" % constant_value)

fp.close()
# Documentation for party
fp = open(code_dir + "/atrinik_party.c", "r")

in_fields = False
in_constants = False

for line in fp:
	if line == "party_fields_struct party_fields[] =\n":
		in_fields = True
		doc_fp.write("\n@page plugin_python_party_fields Python party fields\nList of the party fields and their meaning:\n")
	elif line == "};\n":
		in_fields = False
		in_constants = False
	elif in_fields:
		parts = line.split(",")

		if len(parts) < 5:
			continue

		field_name = parts[0].strip()[2:-1]
		field_type = parts[1].strip()
		field_offset = parts[2].strip()[9:]
		field_offset2 = parts[3].strip()[:-1]

		doc_fp.write("\n- <b>%s</b>: (%s) @copydoc %s::%s" % (field_name, fieldtype_to_string(field_type), field_offset, field_offset2))
	elif line == "static Atrinik_Constant party_constants[] =\n":
		in_constants = True
		doc_fp.write("\n@page plugin_python_party_constants Python party constants\nList of the party constants and their meaning:\n")
	elif in_constants:
		parts = line.split(",")

		if len(parts) < 2:
			continue

		constant_name = parts[0].strip()[1:]

		if constant_name == "NULL":
			continue

		constant_name = constant_name[1:-1]
		constant_value = parts[1].strip()[:-1]

		doc_fp.write("\n- <b>%s</b>: " % constant_name)

		if constant_value.isdigit():
			doc_fp.write("%s" % constant_value)
		else:
			doc_fp.write("@copydoc %s" % constant_value)

fp.close()
# Documentation for the plugin in general
fp = open(code_dir + "/plugin_python.c", "r")

in_constants = False

for line in fp:
	if line == "};\n":
		in_constants = False
	elif line == "static Atrinik_Constant module_constants[] =\n":
		in_constants = True
		doc_fp.write("\n@page plugin_python_constants Python constants\nList of the general Python constants and their meaning:\n")
	elif in_constants:
		parts = line.split(",")

		if len(parts) < 2:
			continue

		constant_name = parts[0].strip()[1:]

		if constant_name == "NULL":
			continue

		constant_name = constant_name[1:-1]
		constant_value = parts[1].strip()[:-1]

		doc_fp.write("\n- <b>%s</b>: " % constant_name)

		if constant_value.isdigit():
			doc_fp.write("%s" % constant_value)
		else:
			doc_fp.write("@copydoc %s" % constant_value)

fp.close()

# Documentation for region
fp = open(code_dir + "/atrinik_region.c", "r")

in_fields = False

for line in fp:
	if line == "region_fields_struct region_fields[] =\n":
		in_fields = True
		doc_fp.write("\n@page plugin_python_region_fields Python region fields\nList of the region fields and their meaning:\n")
	elif line == "};\n":
		in_fields = False
	elif in_fields:
		parts = line.split(",")

		if len(parts) < 5:
			continue

		field_name = parts[0].strip()[2:-1]
		field_type = parts[1].strip()
		field_offset = parts[2].strip()[9:]
		field_offset2 = parts[3].strip()[:-2]

		doc_fp.write("\n- <b>%s</b>: (%s) @copydoc %s::%s" % (field_name, fieldtype_to_string(field_type), field_offset, field_offset2))

fp.close()

doc_fp.write("\n*/\n")
doc_fp.close()

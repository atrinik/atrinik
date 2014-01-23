# This script is used to generate documentation about field/flag
# usage of different object types. Unlike the Gridarta types.xml
# convert tool, types-extra should include everything, even
# internally used, to make it easier for developers to see unused
# fields/flags, and to understand what they do.

fp = open("types-extra", "r")

last_field = None
last_type = None
types = {}
definitions = {}

def preprocess(string):
    for definition in definitions:
        string = string.replace(definition, definitions[definition])

    return string

for line in fp:
    if line[:1] == "#" and not last_type:
        if line[:8] == "#define ":
            definition_name = line[8:line.find(" ", 8)]
            definition_content = line[8 + len(definition_name):].strip()
            definitions[definition_name] = definition_content

        continue

    if line[:5] == "type ":
        last_type = line[5:].strip()
        types[last_type] = {
            "fields": [],
        }
    elif line == "end\n":
        last_field = None
        last_type = None
    elif line != "\n":
        if last_field and line[:6] == "field ":
            last_field = None

        if line[:6] == "field ":
            last_field = line[6:].strip()
            types[last_type]["fields"].append({
                "field": last_field,
                "explanation": "",
                "field_help": None,
            })
        elif line[:11] == "field_help ":
            types[last_type]["fields"][len(types[last_type]["fields"]) - 1]["field_help"] = line[11:].strip()
        else:
            types[last_type]["fields"][len(types[last_type]["fields"]) - 1]["explanation"] += line

fp.close()

doc_fp = open("types-extra.dox", "w")

for type in types:
    doc_fp.write("/**\n@def %s\n@ref ::obj \"Object\" fields and flags used by this type." % type)
    doc_fp.write("\n<table>\n<tr>\n<th>Field/Flag</th>\n<th>Explanation</th>\n</tr>")

    for i in range(0, len(types[type]["fields"])):
        doc_fp.write("\n<tr>\n<td>")

        if types[type]["fields"][i]["field_help"]:
            doc_fp.write("<span title=\"%s\" style=\"border-bottom: 1px dashed #000;\">" % preprocess(types[type]["fields"][i]["field_help"]))

        if types[type]["fields"][i]["field"][:5] == "FLAG_":
            doc_fp.write("@ref ")

        doc_fp.write(types[type]["fields"][i]["field"])

        if types[type]["fields"][i]["field_help"]:
            doc_fp.write("</span>")

        doc_fp.write("</td>\n<td>%s</td>\n</tr>" % types[type]["fields"][i]["explanation"].strip())

    doc_fp.write("\n</table>\n*/\n\n")

doc_fp.close()

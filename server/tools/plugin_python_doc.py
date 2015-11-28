# @file
# This script will look in Python plugin source files for fields,
# flags and constants to transform to Doxygen documentation.

from CParser import CParser, isint
import os
import math

# Transform fieldtype macro type to something that can be understood by
# scripters.
# @param fieldtype The field type, as found in the code.
# @return Easier-to-understand version of the field type.
def fieldtype_to_string(fieldtype):
    if fieldtype == "FIELDTYPE_CSTR" or fieldtype == "FIELDTYPE_SHSTR" or fieldtype == "FIELDTYPE_CARY":
        return "string"
    elif fieldtype == "FIELDTYPE_OBJECT" or fieldtype == "FIELDTYPE_OBJECTREF" or fieldtype == "FIELDTYPE_OBJECT2":
        return "object"
    elif fieldtype == "FIELDTYPE_UINT64" or fieldtype == "FIELDTYPE_SINT64" or fieldtype == "FIELDTYPE_UINT32" or fieldtype == "FIELDTYPE_SINT32" or fieldtype == "FIELDTYPE_UINT16" or fieldtype == "FIELDTYPE_SINT16" or fieldtype == "FIELDTYPE_UINT8" or fieldtype == "FIELDTYPE_SINT8":
        return "integer"
    elif fieldtype == "FIELDTYPE_FLOAT":
        return "float"
    elif fieldtype == "FIELDTYPE_MAP":
        return "map"
    elif fieldtype == "FIELDTYPE_REGION":
        return "region"
    elif fieldtype == "FIELDTYPE_PARTY":
        return "party"
    elif fieldtype == "FIELDTYPE_PLAYER":
        return "player"
    elif fieldtype == "FIELDTYPE_ARCH":
        return "archetype"
    elif fieldtype == "FIELDTYPE_BOOLEAN":
        return "boolean"
    elif fieldtype == "FIELDTYPE_ANIMATION":
        return "animation"
    elif fieldtype == "FIELDTYPE_FACE":
        return "face"

    return "unknown"

# Fix some @copydoc references so Doxygen can linkify them properly.
# @param s @copydoc reference.
# @return Modified string.
def fix_copydoc(s):
    if s[:14] == "object::stats.":
        return "living::" + s[14:]
    elif s[:15] == "player::socket.":
        return "socket_struct::" + s[15:]

    return s

# Make a <span> which works similar to <acronym>.
# @param s String.
# @param t Title-text.
# @return The <span>.
def make_span_hover(s, t):
    return "<span title=\"{0}\" style=\"border-bottom: 1px dashed #000; cursor: help;\">{1}</span>".format(t, s)

# Initialize the CParser.
parser = CParser({
    "defines": [
        "FLAG_(.*)",
        "MAP_FLAG_(.*)",
    ],
})

# Go through the source code files recursively, and parse them.
# @param path Path.
def parse_rec(path):
    nodes = os.listdir(path)

    for node in nodes:
        if os.path.isdir(path + "/" + node):
            parse_rec(path + "/" + node)
        elif os.path.isfile(path + "/" + node):
            if node[-2:] == ".c" or node[-2:] == ".h":
                parser.parse(path + "/" + node)

parse_rec("../src")

doc_fp = open("../doc/plugin_python_doc.dox", "w")

for comment in parser.cparser_comments:
    doc_fp.write("/**\n" + comment["comment"])

    if not comment["match"]:
        data = comment["data"][0]["contents"]

        doc_fp.write("\n<table>")

        if comment["data"][0]["name"] == "fields":
            doc_fp.write("\n\t<tr>")
            doc_fp.write("\n\t\t<th width=\"10%\">Name</th>")
            doc_fp.write("\n\t\t<th width=\"5%\">Type</th>")
            doc_fp.write("\n\t\t<th width=\"45%\">Details</th>")
            doc_fp.write("\n\t\t<th width=\"5%\">Flags</th>")
            doc_fp.write("\n\t\t<th width=\"35%\">Notes</th>")
            doc_fp.write("\n\t</tr>")

            for mem in data:
                doc_fp.write("\n\t<tr>")
                doc_fp.write("\n\t\t<td>{0}</td>".format(mem["contents"][0][1:-1]))
                doc_fp.write("\n\t\t<td>{0}</td>".format(fieldtype_to_string(mem["contents"][1])))
                doc_fp.write("\n\t\t<td>@copydoc {0} </td>".format(fix_copydoc(mem["contents"][2][9:] + "::" + mem["contents"][3][:-1])))
                doc_fp.write("\n\t\t<td>")

                if mem["contents"][4] == "FIELDFLAG_READONLY":
                    doc_fp.write("&nbsp;(" + make_span_hover("readonly", "Cannot be modified.") + ")")
                elif mem["contents"][4] == "FIELDFLAG_PLAYER_READONLY":
                    doc_fp.write("&nbsp;(" + make_span_hover("player&nbsp;readonly", "Cannot be modified if object is a player.") + ")")
                elif mem["contents"][4] == "FIELDFLAG_PLAYER_FIX":
                    doc_fp.write("&nbsp;(" + make_span_hover("player&nbsp;fix", "Will fix player after modifying this field.") + ")")

                doc_fp.write("</td>")
                doc_fp.write("\n\t\t<td>{0}</td>".format("comment" in mem and mem["comment"] or ""))
                doc_fp.write("\n\t</tr>")

        elif comment["data"][0]["name"].startswith("constants"):
            doc_fp.write("\n\t<tr>")
            doc_fp.write("\n\t\t<th width=\"10%\">Name</th>")
            doc_fp.write("\n\t\t<th width=\"50%\">Details</th>")
            doc_fp.write("\n\t\t<th width=\"40%\">Notes</th>")
            doc_fp.write("\n\t</tr>")

            for mem in data:
                if mem["contents"][0] == "NULL":
                    continue

                doc_fp.write("\n\t<tr>")
                doc_fp.write("\n\t\t<td>{0}</td>".format(mem["contents"][0][1:-1]))

                if isint(mem["contents"][1]):
                    doc_fp.write("\n\t\t<td>{0}</td>".format(mem["contents"][1]))
                else:
                    doc_fp.write("\n\t\t<td>@copydoc {0} </td>".format(mem["contents"][1]))
                doc_fp.write("\n\t\t<td>{0}</td>".format("comment" in mem and mem["comment"] or ""))
                doc_fp.write("\n\t</tr>")

        doc_fp.write("\n</table>")
    elif comment["match"] and comment["match"] in parser.matches:
        flags = {}
        i = 0
        import pdb
        from pprint import pprint

        for (constant, val) in parser.matches[comment["match"]]:
            if isint(val):
                val = int(val)

                if constant[:9] == "MAP_FLAG_":
                    val = math.log2(val)

                flags[val] = constant

        data = comment["data"][0]["contents"]

        doc_fp.write("\n<table>")
        doc_fp.write("\n\t<tr>")
        doc_fp.write("\n\t\t<th width=\"10%\">Name</th>")
        doc_fp.write("\n\t\t<th width=\"90%\">Details</th>")
        doc_fp.write("\n\t</tr>")

        i = -1

        for mem in data:
            for val in mem["contents"]:
                i += 1

                if val == "NULL":
                    continue

                val = val[1:-1]

                if comment["data"][0]["name"] == "object_flag_names":
                    val = "f_" + val

                doc_fp.write("\n\t<tr>")
                doc_fp.write("\n\t\t<td>{0}</td>".format(val))
                doc_fp.write("\n\t\t<td>@copydoc {0} </td>".format(flags[i]))
                doc_fp.write("\n\t</tr>")

        doc_fp.write("\n</table>")

    doc_fp.write("\n*/\n")

doc_fp.close()

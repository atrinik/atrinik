# @file
# A very basic C code parser.

import re

# Check whether the passed string is an integer.
# @param s String.
# @return True if s is an integer, False otherwise.
def isint(s):
    try:
        int(s)
        return True
    except ValueError:
        return False

ARRAY_PARSE_DATA_PATTERN = re.compile(r"^\{(.*)\}[,]?$")
PARSE_ARRAY_PATTERN = re.compile(r"([a-zA-Z0-9_ \*]*)\[.*\] =")
PARSE_ARRAY_SUB_PATTERN = re.compile(r"[\*]")
PARSE_ENUM_REC_PATTERN = re.compile(r"^([\w_]+)\s*(?:=\s*(\d+)\s*)?(?:,)?\s*$",
                                    re.I)
PARSE_ENUM_PATTERN = re.compile(r"^(typedef)?\s*enum\s*([\w_]*)\s*({)?$")
PARSE_SINGLE_COMMENT_PATTERN = re.compile(r"(.+?(?=//|$))(?://(?:/<)?\s*(.*))")


# Simple C code parser.
class CParser(object):
    # Initialize the CParser.
    def __init__(self):
        self.matches = {}
        self.cparser_comments = []

    # Split array's data by commas.
    # @param line Array data to split.
    # @return List of the split data, free of whitespace.
    @staticmethod
    def array_split_data(line):
        return [x.strip() for x in line.split(",") if x.strip()]

    # Parses an array's data. This most likely won't work for various array
    # styles, but it should work well enough for our purpose.
    # @param line Line to parse data from.
    # @return Parsed data.
    def array_parse_data(self, line):
        l = ARRAY_PARSE_DATA_PATTERN.findall(line)

        if l:
            line = l[0]

        return self.array_split_data(line)

    # Recursively parse an array.
    # @return Data about the array.
    def parse_array_rec(self, in_array=False):
        data = []
        tmp_data = {}

        for line in self.fh:
            line = line.strip()

            # We're not in an array already, and this is the start of one.
            if not in_array and line.startswith("{"):
                in_array = True
                continue
            # End of the array.
            elif line.endswith(";"):
                break

            comment = self.parse_comment(line)
            if comment is not None:
                tmp_data["comment"] = comment
                continue

            if not line.strip():
                continue

            # Parse its data.
            l = self.array_parse_data(line)
            tmp_data["contents"] = l
            data.append(tmp_data)
            tmp_data = {}

        return data

    # Try to match a line as an array.
    # @param line Line to try to match.
    # @return None if we didn't match (or we don't want to look for
    # this array).
    def parse_array(self, line):
        # Construct a regex that should match most arrays for our needs.
        m = PARSE_ARRAY_PATTERN.match(line)
        if m is None:
            return None

        # Everything before '['.
        beginning = m.group(1)
        # Split the above into parts.
        parts = beginning.split()
        # Get the last element of 'parts', which is name of the array, and
        # remove asterisks.
        name = PARSE_ARRAY_SUB_PATTERN.sub("", parts[-1])

        # Parse it.
        l = self.parse_array_rec(in_array=line.endswith("{"))
        return [(name, {"data": l})]

    def parse_enum_rec(self, in_enum=False):
        data = []
        obj = {"comment": ""}
        last_val = 0

        for line in self.fh:
            line = line.strip()
            if not line:
                continue

            # We're not in an enum already, and this is the start of one.
            if not in_enum and line.startswith("{"):
                in_enum = True
                continue
            # End of the enum.
            elif line.endswith(";"):
                break

            comment = self.parse_comment(line)
            if comment is not None:
                obj["comment"] = comment
                continue

            line, comment = self.parse_single_comment(line)
            if not line:
                return None

            match = PARSE_ENUM_REC_PATTERN.match(line)
            if match is None:
                continue

            if match.group(2):
                last_val = int(match.group(2))

            obj["data"] = last_val

            if comment:
                obj["comment"] = comment

            data.append((match.group(1), obj))
            obj = {"comment": ""}

            last_val += 1

        return data

    def parse_enum(self, line):
        m = PARSE_ENUM_PATTERN.match(line)
        if m is None:
            return None

        # Parse it.
        return self.parse_enum_rec(in_enum=line.endswith("{"))

    # Parse a comment from line.
    # @param line Line.
    # @return The parsed comment.
    def parse_comment(self, line):
        # Not starting with '/*', so not a comment.
        if not line.startswith("/*"):
            return None

        # Find the comment's end.
        comment_end = line.find("*/")
        parts = []

        # If we didn't find an end, try to find it.
        if comment_end == -1:
            parts.append(line[2:].strip())

            for line in self.fh:
                # The comment's end.
                comment_end = line.find("*/")
                # Append it to the list.
                parts.append(line[:comment_end].strip())

                # Was the end reached?
                if comment_end != -1:
                    break
        else:
            parts.append(line[2:comment_end].strip())

        # Go through the parts to do any final adjustements.
        for i, part in enumerate(parts):
            # Often enough, comments are prefixed with an asterisk, so get
            # rid of it.
            if part.startswith("*"):
                parts[i] = part[1:].strip()

        # Remove empty strings from the list.
        parts = filter(lambda x: len(x) > 0, parts)

        # Return the list, with parts joined using a newline.
        return "\n".join(parts)

    @staticmethod
    def parse_single_comment(line):
        match = PARSE_SINGLE_COMMENT_PATTERN.match(line)
        if match is None:
            return line, None

        return match.group(1).strip(), match.group(2)

    # Check if a #define was encountered.
    # @param line Line to check in.
    # @return True if it was a #define, False otherwise.
    def parse_define(self, line):
        # Must begin with a '#'.
        if not line.startswith("#"):
            return None

        # Defines can have any number of whitespace after '#', so we'll have
        # to strip it off first.
        line = line[1:].strip()

        # Is it really a define?
        if not line.startswith("define "):
            return None

        line, comment = self.parse_single_comment(line)
        if not line:
            return None

        # Now we know it's a define, but we don't know its name. So, now we
        # need to strip any whitespace after "define ".
        line = line[7:].strip()

        macro_pos = line.find("(")
        space_pos = line.find(" ")
        backslash_pos = line.find("\\")

        # No '(' character or no space?
        if macro_pos == -1 or space_pos == -1:
            # Macro is set, so everything before the '(' is part of the name.
            if macro_pos != -1:
                name_pos = macro_pos
            # Space is set.
            elif space_pos != -1:
                name_pos = space_pos
            # Not multiline, so it's something like '#define xyz', which contains
            # no data, so we don't need it.
            elif backslash_pos == -1:
                return None
            # Otherwise the name is everything until the backslash character.
            else:
                name_pos = backslash_pos
        # We have macro and a space.
        else:
            # Prefer macro if it's the first, otherwise the space.
            name_pos = macro_pos < space_pos and macro_pos or space_pos

        # Our name.
        name = line[:name_pos].strip()

        # Parts this define is made of.
        parts = [line[name_pos:].strip()]

        # If we found a backslash character...
        if backslash_pos != -1:
            # ... go through the lines, looking for more lines that are part
            # of this define.
            for line in self.fh:
                # Look for a backslash.
                backslash_pos = line.find("\\")
                # Add it to the define's parts.
                parts.append(line[:backslash_pos].strip())

                # Is this the last line of the define?
                if backslash_pos == -1:
                    break

        obj = {"data": "\n".join(parts)}

        if comment is not None:
            obj["comment"] = comment

        return [(name, obj)]

    # Parse a single file.
    # @param file Name of the file to parse.
    def parse(self, file):
        matches = {}
        self.fh = open(file)
        last_comment = None

        for line in self.fh:
            line = line.strip()

            comment = self.parse_comment(line)
            if comment is not None:
                last_comment = comment
                continue

            objects = self.parse_define(line)

            if objects is None:
                objects = self.parse_array(line)

            if objects is None:
                objects = self.parse_enum(line)

            if objects:
                for obj in objects:
                    if "comment" not in obj[1]:
                        obj[1]["comment"] = last_comment
                        last_comment = None

                    matches[obj[0]] = obj[1]

        self.fh.close()
        return matches

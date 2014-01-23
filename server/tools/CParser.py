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

# Simple C code parser.
class CParser:
    # Initialize the CParser.
    # @param Array names, constants, etc that we are looking for.
    def __init__(self, looking_for):
        self.looking_for = looking_for
        self.matches = {}
        self.cparser_comments = []

    # Check if string 's' did make a match by looking in self.looking_for.
    # @param s The string.
    # @param t Member of self.looking_for to look in.
    # @return Match name if match was made, None otherwise.
    def did_make_match(self, s, t):
        for match in self.looking_for[t]:
            if re.match(match, s):
                return match

        return None

    # Split array's data by commas.
    # @param line Array data to split.
    # @return List of the split data, free of whitespace.
    def array_split_data(self, line):
        return list(filter(lambda x: len(x) > 0, [x.strip() for x in line.split(",")]))

    # Parses an array's data. This most likely won't work for various array
    # styles, but it should work well enough for our purpose.
    # @param line Line to parse data from.
    # @return Parsed data.
    def array_parse_data(self, line):
        # Construct a regex.
        l = re.findall("^\{(.*)\}[,]?$", line)

        if l:
            line = l[0]

        return self.array_split_data(line)

    # Recursively parse an array.
    # @return Data about the array.
    def parse_array_rec(self):
        data = []
        # Are we in an array?
        in_array = False
        tmp_data = {}

        for line in self.fh:
            line = line.strip()

            # We're not in an array already, and this is the start of one.
            if not in_array and line[:1] == "{":
                in_array = True
                continue
            # End of the array.
            elif line[:2] == "};":
                in_array = False
                break

            comment = self.is_comment(line)

            if comment:
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
    # @param must_match If True, the array name must match one of the
    # array names in self.looking_for["arrays"].
    # @return None if we didn't match (or we don't want to look for
    # this array).
    def parse_array(self, line, must_match = True):
        # Construct a regex that should match most arrays for our needs.
        m = re.match("([a-zA-Z0-9_ \*]*)\[.*\] =", line)

        # Did it match?
        if m:
            # Everything before '['.
            beginning = m.group(1)
            # Split the above into parts.
            parts = beginning.split()
            # Get the last element of 'parts', which is name of the array, and
            # remove asterisks.
            name = re.sub("[\*]", "", parts[-1])

            # Parse it.
            l = self.parse_array_rec()

            if must_match and not self.did_make_match("name", "arrays"):
                return None

            return {"name": name, "contents": l,}

        return None

    # Trigger a special "@cparser" comment.
    # @param comment The comment that was used to trigger this, without
    # the "@cparser".
    def trigger_cparser(self, comment):
        parts = comment.split("\n")
        match = parts.pop(0)
        data = {
            "comment": "\n".join(parts).strip(),
            "match": match.strip(),
            "data": [],
        }

        for line in self.fh:
            # Is this a comment?
            comment = self.is_comment(line)

            # Not a comment.
            if not comment:
                # Try to parse the line as an array.
                arr = self.parse_array(line, False)

                # Success!
                if arr:
                    data["data"].append(arr)
            # "@endcparser" marks the end of previous "@cparser" command.
            elif comment == "@endcparser":
                break

        self.cparser_comments.append(data)

    # Parse a comment from line.
    # @param line Line.
    # @return The parsed comment.
    def parse_comment(self, line):
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
            if part[:1] == "*":
                parts[i] = part[1:].strip()

        # Remove empty strings from the list.
        parts = list(filter(lambda x: len(x) > 0, parts))

        # Return the list, with parts joined using a newline.
        return "\n".join(parts)

    # Is the line a comment? If so, parse the comment (even if it is
    # spanning across lines), do any processing, and return it.
    # @param line The line.
    # @param None if this is not a comment, the comment otherwise.
    def is_comment(self, line):
        line = line.strip()

        # Not starting with '/*', so not a comment.
        if line[:2] != "/*":
            return None

        # Parse it.
        comment = self.parse_comment(line)

        # '@cparser' command?
        if comment[:8] == "@cparser":
            self.trigger_cparser(comment[8:])

        return comment

    # Check if a #define was encountered.
    # @param line Line to check in.
    # @return True if it was a #define, False otherwise.
    def line_check_define(self, line):
        # Must begin with a '#'.
        if line[:1] != "#":
            return False

        # Defines can have any number of whitespace after '#', so we'll have
        # to strip it off first.
        line = line[1:].strip()

        # Is it really a define?
        if line[:7] != "define ":
            return False

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
                return False
            # Otherwise the name is everything until the backslash character.
            else:
                name_pos = backslash_pos
        # We have macro and a space.
        else:
            # Prefer macro if it's the first, otherwise the space.
            name_pos = macro_pos < space_pos and macro_pos or space_pos

        # Our name.
        name = line[:name_pos].strip()

        # Check if match was made.
        m_name = self.did_make_match(name, "defines")

        if not m_name:
            return False

        if not m_name in self.matches:
            self.matches[m_name] = []

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

        # Add it to the dictionary of the matches found, with the parts
        # combined into a single string.
        self.matches[m_name].append((name, "\n".join(parts)))
        return True

    # Try to make use of data in a single line.
    # @param line The line.
    def line_match(self, line):
        # Strip off left and right whitespace.
        line = line.strip()

        # Try to match a define.
        if "defines" in self.looking_for:
            self.line_check_define(line)

    # Parse a single file.
    # @param file Name of the file to parse.
    def parse(self, file):
        self.fh = open(file, "r")

        for line in self.fh:
            if not self.is_comment(line):
                self.line_match(line)

        self.fh.close()

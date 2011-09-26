## @file
## Deals with markup-related operations.

## The markup escape table.
_markup_escape_table = {
	">": "&gt;",
	"<": "&lt;",
}

## Escapes markup in the specified string.
## @param text The string.
## @return Escaped string.
def markup_escape(text):
	return "".join(_markup_escape_table.get(c, c) for c in text)

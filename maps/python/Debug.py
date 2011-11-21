## @file
## Contains debugging related code.

## Figure out the attribute differences between the specified objects.
## @return List of attribute names that are different.
def objs_get_diff(*args):
	results = []
	l = []

	for arg in args:
		l += dir(arg)

	for attr in set(l):
		if attr.startswith("_") or not attr.islower():
			continue

		# Ignore some attributes.
		if attr == "count" or attr == "below" or attr == "above":
			continue

		for arg in args:
			for arg2 in args:
				if arg != arg2 and getattr(arg, attr) != getattr(arg2, attr):
					results.append(attr)

	return list(set(results))

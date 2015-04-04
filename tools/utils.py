__author__ = "Alex Tokar"
__copyright__ = "Copyright (c) 2009-2015 Atrinik Development Team"
__credits__ = ["Alex Tokar"]
__license__ = "GPL"
__version__ = "2.0"
__maintainer__ = "Alex Tokar"
__email__ = "admin@atokar.net"

import os
from collections import OrderedDict


def find_files(where, ext=None, rec=True, ignore_dirs=True, ignore_files=False,
               ignore_paths=None):
    """
    Find files.

    :param where: Directory to search.
    :tyoe where: str or unicode
    :param ext: Only find files with this extension.
    :type ext: NoneType or str or unicode
    :param rec: Whether to search recursively.
    :type rec: bool
    :param ignore_dirs: Ignore directories.
    :type ignore_dirs: bool
    :param ignore_files: Ignore files.
    :type ignore_files: bool
    :param ignore_paths: Paths to ignore.
    :type ignore_paths: NoneType or list or tuple
    :returns: List containing the found files.
    :rtype list
    """

    nodes = os.listdir(where)
    files = []

    for node in nodes:
        path = os.path.join(where, node)

        # Do we want to ignore this path?
        if ignore_paths and path in ignore_paths:
            continue

        # A directory.
        if os.path.isdir(path):
            # Do we want to go on recursively?
            if rec:
                files += find_files(path, ext)

            # Are we ignoring directories? If not, add it to the list.
            if not ignore_dirs:
                files.append(path)
        else:
            # Only add the file if we're not ignoring files and ext was not set
            # or it matches.
            if not ignore_files and (ext is None or path.endswith(ext)):
                files.append(path)

    return files


def dump_dict(d, pretty=False, _indent_level=0):
    """
    Dumps specified dictionary as a string.

    :param d: Dictionary to dump.
    :type d: dict or OrderedDict
    :param pretty: Whether to pretty-print.
    :type pretty: bool
    :param _indent_level: Indent level.
    :type _indent_level: int
    :returns: Dumped dictionary.
    :rtype str or unicode
    """
    s = "OrderedDict((" if type(d) is OrderedDict else "{"

    if pretty:
        s += "\n"

    for key in d:
        if pretty:
            s += " " * 4 * (_indent_level + 1)

        if type(d) is OrderedDict:
            s += "({}, ".format(repr(key))
        else:
            s += "{}: ".format(repr(key))

        if isinstance(d[key], (dict, OrderedDict)):
            s += dump_dict(d[key], pretty, _indent_level + 1)
        else:
            s += "{}".format(repr(d[key]))

        if type(d) is OrderedDict:
            s += ")"

        s += ","

        if pretty:
            s += "\n"

    if pretty:
        s += " " * 4 * _indent_level

    s += "))" if type(d) is OrderedDict else "}"

    return s


def file_copy(path, output):
    """
    Copies contents of file specified by 'path' into 'output', stripping
    whitespace, empty and commented out lines.
    :param path: Path to file that should be copied.
    :type path: str or unicode
    :param output: File handle to write to.
    :type output: io.FileIO or io.StringIO
    """

    with open(path) as orig_file:
        for line in orig_file:
            # Blank line or comment.
            if not line or line.startswith("#"):
                continue

            output.write("{}\n".format(line.rstrip()).encode())

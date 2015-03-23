#!/usr/bin/python
#*************************************************************************
#*            Atrinik, a Multiplayer Online Role Playing Game            *
#*                                                                       *
#*    Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team    *
#*                                                                       *
#* Fork from Crossfire (Multiplayer game for X-windows).                 *
#*                                                                       *
#* This program is free software; you can redistribute it and/or modify  *
#* it under the terms of the GNU General Public License as published by  *
#* the Free Software Foundation; either version 2 of the License, or     *
#* (at your option) any later version.                                   *
#*                                                                       *
#* This program is distributed in the hope that it will be useful,       *
#* but WITHOUT ANY WARRANTY; without even the implied warranty of        *
#* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
#* GNU General Public License for more details.                          *
#*                                                                       *
#* You should have received a copy of the GNU General Public License     *
#* along with this program; if not, write to the Free Software           *
#* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
#*                                                                       *
#* The author can be reached at admin@atrinik.org                        *
#*************************************************************************

import os

PROJECT_DIRS = ["../server", "../client"]

# Find files in the specified path.
# @param where Where to look for the files.
# @param ext What the file must end with.
# @param rec Whether to go on recursively.
# @param ignore_dirs Whether to ignore directories.
# @param ignore_files Whether to ignore files.
# @param ignore_paths What paths to ignore.
# @return A list containing files/directories found based on the set criteria.
def find_files(where, ext = None, rec = True, ignore_dirs = True, ignore_files = False, ignore_paths = None):
    nodes = os.listdir(where)
    nodes.sort()
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
            # Only add the file if we're not ignoring files and ext was not set or it matches.
            if not ignore_files and (not ext or path.endswith(ext)):
                files.append(path)

    return files

for project_dir in PROJECT_DIRS:
    path = os.path.join(project_dir, "src")
    ignore_paths = [os.path.join(path, "updater"), os.path.join(path, "plugins"), os.path.join(path, "modules"), os.path.join(path, "tests")]

    files = find_files(path, ".c", ignore_paths = [os.path.join(path, "toolkit")] + ignore_paths)

    fp = open(os.path.join(path, "cmake.txt"), "w")
    fp.write("set(SOURCES\n\t{}\n\t${{SOURCES_TOOLKIT}})".format("\n\t".join(["/".join(f.split(os.path.sep)[len(project_dir.split(os.path.sep)):]) for f in files])))
    fp.close()


/*******************************************************************************
 *               Atrinik, a Multiplayer Online Role Playing Game               *
 *                                                                             *
 *       Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team       *
 *                                                                             *
 * This program is free software; you can redistribute it and/or modify it     *
 * under the terms of the GNU General Public License as published by the Free  *
 * Software Foundation; either version 2 of the License, or (at your option)   *
 * any later version.                                                          *
 *                                                                             *
 * This program is distributed in the hope that it will be useful, but WITHOUT *
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       *
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for    *
 * more details.                                                               *
 *                                                                             *
 * You should have received a copy of the GNU General Public License along     *
 * with this program; if not, write to the Free Software Foundation, Inc.,     *
 * 675 Mass Ave, Cambridge, MA 02139, USA.                                     *
 *                                                                             *
 * The author can be reached at admin@atrinik.org                              *
 ******************************************************************************/

/**
 * @file
 * Artifact parser implementation.
 */

#include <fstream>
#include <boost/lexical_cast.hpp>

#include <artifact_parser.h>
#include <artifact_object.h>
#include <logger.h>

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

void ArtifactParser::load(const std::string& path)
{
    BOOST_LOG_FUNCTION();

    ArtifactObjectManager::setup();
    LOG(Detail) << "Loading artifacts from: " << path;
    ifstream file(path);

    if (!file.is_open()) {
        ArtifactObjectManager::setup(false);
        throw LOG_EXCEPTION(runtime_error("could not open file"));
    }

    ArtifactObjectPtr artifact;
    string name;
    bool in_obj, none;

    parse(file,
            [&artifact, &file, &name, &in_obj, &none] (const std::string & key,
            const std::string & val) mutable -> bool
            {
                if (key.empty() && val == "end") {
                    ArtifactObjectManager::add(artifact,
                            none ? -1 : artifact->obj()->getprimaryinstance());
                    artifact.reset();
                    return true;
                } else if (key == "artifact") {
                    name = val;
                    return true;
                } else if (key == "Allowed") {
                    artifact.reset(new ArtifactObject());
                    in_obj = false;
                    none = val == "none";
                    // Fall through to the load part
                } else if (!artifact) {
                    LOG(Error) << "Unrecognized attribute (before artifact "
                            "definition): " << key << " " << val;
                    ArtifactObjectManager::setup(false);
                    throw LOG_EXCEPTION(runtime_error(
                            "corrupted artifacts file"));
                } else if (key.empty() && val == "Object") {
                    in_obj = true;
                    return true;
                } else if (in_obj) {
                    if (!artifact->load_object(key, val)) {
                        return true; // TODO: remove
                        LOG(Error) << "Unrecognized attribute: " << key <<
                                " " << val;
                        ArtifactObjectManager::setup(false);
                        throw LOG_EXCEPTION(runtime_error(
                                "corrupted artifacts file"));
                    }

                    return true;
                }

                if (!artifact->load(key, val)) {
                    LOG(Error) << "Unrecognized attribute: " << key << " " <<
                            val;
                    ArtifactObjectManager::setup(false);
                    throw LOG_EXCEPTION(runtime_error(
                            "corrupted artifacts file"));
                } else if (key == "def_arch") {
                    // This is a special case to set the artifact object's
                    // archetype name.
                    artifact->load("artifact", name);
                }

                return true;
            }
    );

    LOG(Detail) << "Loaded " << ArtifactObjectManager::count() <<
            " artifacts";
    ArtifactObjectManager::setup(true);
}

}

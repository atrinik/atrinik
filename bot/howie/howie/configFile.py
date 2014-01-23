"""
Handles the reading, writing of (and general access to) Howie's
persistent configuration information, stored (by default) in
the howie.ini file.
"""

import os
import string
import sys

import ConfigParser

configFileName = "howie.ini"

# Load default values for various config-file options.
# The defaults are scattered throughout the frontend and
# backend modules, each of which may have a dictionary
# attribute called configDefaults similar to the one
# below.  This code merges all the configDefaults into
# one master copy.
configDefaults = {
    "general.logging":              "no",
    "general.logdir":               "logs",
    "general.maxdelay":             "0",
    "general.sessionspersist":      "no",
    "general.sessionsdir":          "sessions",
    "general.verbose":              "no",
    }


def _WriteINI(file, config=configDefaults):
    """
    Writes the config dictionary to a .INI file.
    """
    if len(config) == 0:
        return
    iniFile = open(file, "w")
    currentSection = ""
    keys = config.keys()
    keys.sort()
    for key in keys:
        # Don't write entries with no value
        value = config[key]
        if value == "":
            continue

        # Split the key into section and variable.        
        section, var = string.split(key, ".", 1)
        if section != currentSection:
            # start a new section
            if currentSection:
                iniFile.write("\n")
            iniFile.write("[%s]\n" % section)
            currentSection = section

        # write the entry
        iniFile.write("%s = %s\n" % (var, value))
    iniFile.close()

def _ReadINI(file, config={}):
    """
    Returns a dictionary with keys of the form
    <section>.<option> and the corresponding values.
    """
    config = config.copy()
    cp = ConfigParser.ConfigParser()
    cp.read(file)
    for sec in cp.sections():
        name = sec.lower()
        for opt in cp.options(sec):
            config[name + "." + opt.lower()] = string.strip(cp.get(sec, opt))
    return config


_config = None
def load(file):
    """
    Returns a dictionary containing the current configuration information,
    read from the specified .INI file.
    """
    global _config, configDefaults
    try:
        if not os.path.exists(file):
            _WriteINI(file)
        _config = _ReadINI(file, configDefaults)
    except:
        print "ERROR: could not load config file %s" % file
        sys.exit(-1)
    return _config

def get():
    global _config
    if _config is None:
        _config = load(configFileName)
    return _config

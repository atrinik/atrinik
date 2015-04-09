"""
Implements configuration related functions.  
"""

import configparser
import os

# Path to configuration file of this program.
__CONFIG_PATH__ = "~/.map_checker.cfg"


class Config(configparser.SafeConfigParser):
    """
    The Config class is a wrapper around configparser.SafeConfigParserto
    provide convenient loading and saving of the config file.
    """

    def __init__(self):
        super(Config, self).__init__()

        self.config_path = os.path.expanduser(__CONFIG_PATH__)

    def load(self, appdir):
        """
        Loads the configuration.
        @param appdir The directory in which this program is located.
        """
        self.read([os.path.join(appdir, "defaults.cfg"), self.config_path])

        for section in self.sections():
            for option in self.options(section):
                # Adjust options that begin with 'path_'
                if option.startswith("path_"):
                    path = self.get(section, option)

                    # If the path is not absolute, it means it's default relative path
                    # from defaults.cfg config file. Thus, we want to make it absolute.
                    if not os.path.isabs(path):
                        self.set(section, option,
                                 os.path.normpath(os.path.join(appdir, path)))

    def save(self):
        """
        Saves the current config to the user config file (__CONFIG_PATH__)
        """
        f = open(self.config_path, "w")
        self.write(f)
        f.close()

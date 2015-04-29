import json
import os
import system.constants


class Database(object):
    def __init__(self, config, path):
        self.config = config
        self.path = path
        self.db = None
        self.init_database()

    def init_database(self):
        self.db = {
            "files": {},
            "global_objects": {
                str(system.constants.Game.Types.beacon): {},
            },
        }

    def load(self):
        if os.path.isfile(self.path):
            with open(self.path) as fp:
                self.db = json.load(fp)

    def save(self):
        with open(self.path, "w") as fp:
            json.dump(self.db, fp)

    def purge(self):
        self.init_database()
        self.save()

    @property
    def global_objects(self):
        return self.db["global_objects"]

    def file_is_in_maps(self, path):
        maps_path = os.path.realpath(self.config.get("General",
                                                     "path_dir_maps"))
        real_path = os.path.realpath(path)

        return real_path.startswith(maps_path)

    def file_is_modified(self, path):
        mtime = self.db["files"].get(os.path.realpath(path))

        if mtime is not None and os.stat(path).st_mtime == mtime:
            return False

        return True

    def file_set_modified(self, path):
        if not self.file_is_in_maps(path):
            return

        real_path = os.path.realpath(path)
        self.db["files"][real_path] = os.stat(path).st_mtime

        for key in self.db["global_objects"]:
            for name in list(self.db["global_objects"][key].keys()):
                if self.db["global_objects"][key][name] == real_path:
                    del self.db["global_objects"][key][name]

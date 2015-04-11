__author__ = "Alex Tokar"
__copyright__ = "Copyright (c) 2009-2015 Atrinik Development Team"
__credits__ = ["Alex Tokar"]
__license__ = "GPL"
__version__ = "2.0"
__maintainer__ = "Alex Tokar"
__email__ = "admin@atokar.net"

import os.path
import utils


class BaseCompiler(object):
    def __init__(self, paths):
        self.paths = paths

    def compile(self):
        raise NotImplementedError("not implemented")


class ArchetypesCompiler(BaseCompiler):
    def compile(self):
        with open(os.path.join(self.paths["arch"],
                               "archetypes"), "wb") as archetypes_file:
            for path in utils.find_files(self.paths["arch"], ext=".arc"):
                utils.file_copy(path, archetypes_file)


class ImagesCompiler(BaseCompiler):
    def compile(self):
        num_images = 0
        dev_dir = os.path.join(self.paths["arch"], "dev")
        bmaps_path = os.path.join(self.paths["arch"], "bmaps")
        images_path = os.path.join(self.paths["arch"], "atrinik.0")

        with open(bmaps_path, "wb") as bmaps_file, \
                open(images_path, "wb") as images_file:
            # 'bug.101' must be the first entry.
            for path in utils.find_files(dev_dir, ext="bug.101.png") + \
                    sorted(utils.find_files(self.paths["arch"], ext=".png",
                                            ignore_paths=(dev_dir,)),
                           key = lambda s: os.path.basename(s)[:-4]):
                name = os.path.basename(path)[:-4]
                # Write it to the bmaps file.
                bmaps_file.write("{}\n".format(name).encode())

                # Get the image's file size.
                size = os.path.getsize(path)
                # Write information about the image to the atrinik.0 file.
                images_file.write("IMAGE {} {} {}\n".format(num_images, size,
                                                            name).encode())

                with open(path, "rb") as image_file:
                    images_file.write(image_file.read())

                num_images += 1


class AnimationsCompiler(BaseCompiler):
    def compile(self):
        l = []

        for path in utils.find_files(self.paths["arch"], ext=".anim"):
            with open(path) as anim_file:
                for line in anim_file:
                    line = line.strip()

                    # Blank line or comment.
                    if not line or line.startswith("#"):
                        continue

                    if line.startswith("anim "):
                        l.append([line])
                    elif line != "mina":
                        l[len(l) - 1].append(line)

        animations_path = os.path.join(self.paths["arch"], "animations")

        with open(animations_path, "wb") as animations_file:
            for anim in sorted(l, key=lambda node: node[0][5:]):
                for line in anim:
                    animations_file.write("{}\n".format(line).encode())

                animations_file.write("mina\n".encode())


class TreasuresCompiler(BaseCompiler):
    def compile(self):
        treasures_path = os.path.join(self.paths["arch"], "treasures")

        with open(treasures_path, "wb") as treasures_file:
            for path in utils.find_files(self.paths["arch"], ext=".trs") + \
                    utils.find_files(self.paths["maps"], ext=".trs"):
                utils.file_copy(path, treasures_file)


class ArtifactsCompiler(BaseCompiler):
    def compile(self):
        artifacts_path = os.path.join(self.paths["arch"], "artifacts")

        with open(artifacts_path, "wb") as artifacts_file:
            for path in utils.find_files(self.paths["arch"], ext=".art") + \
                    utils.find_files(self.paths["maps"], ext=".art"):
                utils.file_copy(path, artifacts_file)

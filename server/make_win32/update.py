# Simple script to assist in updating project files for CodeBlocks and
# VisualC when new files are added/removed.

import os

directory = "../../src"
fp_cb = open("codeblocks.txt", "w")
vc_data = {}

def scan(directory):
    nodes = os.listdir(directory)

    for node in nodes:
        path = directory + "/" + node

        if os.path.isdir(path):
            if node == "plugins" or node == "tests":
                continue

            if not node in vc_data:
                vc_data[node] = []

            scan(path)
        elif os.path.isfile(path):
            if node == "autoconf.h":
                continue

            if path[-2:] == ".c" or path[-2:] == ".h" or path[-2:] == ".l":
                new_path = path.replace("/", "\\")

                if path[-2:] != ".l":
                    fp_cb.write("\n\t\t<Unit filename=\"..\{0}\">\n\t\t\t<Option compilerVar=\"CC\" />\n\t\t</Unit>".format(new_path))

                if path[-2:] == ".h":
                    vc_data["include"].append(new_path)
                else:
                    vc_data[os.path.basename(os.path.dirname(path))].append(new_path)

scan(directory)
fp_cb.close()

fp_vc = open("visualc-include.txt", "w")
fp_vc.write("\n\t\t<Filter\n\t\t\tName=\"Header Files\"\n\t\t\tFilter=\"h;hpp;hxx;hm;inl;inc;xsd\"\n\t\t\tUniqueIdentifier=\"{93995380-89BD-4b04-88EB-625FBE52EBFB}\"\n\t\t\t>")
vc_data["include"].sort()

for f in vc_data["include"]:
    fp_vc.write("\n\t\t\t<File\n\t\t\t\tRelativePath=\"..\{0}\"\n\t\t\t\t>\n\t\t\t</File>".format(f))

fp_vc.write("\n\t\t</Filter>")
fp_vc.close()

fp_vc = open("visualc-source.txt", "w")
fp_vc.write("\n\t\t<Filter\n\t\t\tName=\"Source Files\"\n\t\t\tFilter=\"cpp;c;cc;cxx;def;odl;idl;hpj;bat;asm;asmx\"\n\t\t\tUniqueIdentifier=\"{4FC737F1-C7A5-4376-A066-2A32D752A2FF}\"\n\t\t\t>")

for d in vc_data:
    if d == "include":
        continue

    vc_data[d].sort()
    fp_vc.write("\n\t\t\t<Filter\n\t\t\t\tName=\"{0}\"\n\t\t\t\t>".format(d))

    for f in vc_data[d]:
        fp_vc.write("\n\t\t\t\t<File\n\t\t\t\t\tRelativePath=\"..\{0}\"\n\t\t\t\t\t>".format(f))

        if f[-2:] == ".l":
            fp_vc.write("\n\t\t\t\t\t<FileConfiguration\n\t\t\t\t\t\tName=\"Debug|Win32\"\n\t\t\t\t\t\t>\n\t\t\t\t\t\t<Tool\n\t\t\t\t\t\t\tName=\"VCCustomBuildTool\"\n\t\t\t\t\t\t\tCommandLine=\"..\\tools\\flex.exe -i -Pyy_{0} -o$(InputDir)$(InputName).c $(InputPath)&#x0D;&#x0A;\"\n\t\t\t\t\t\t\tOutputs=\"$(InputDir)$(InputName).c\"\n\t\t\t\t\t\t/>\n\t\t\t\t\t</FileConfiguration>".format(os.path.basename(f.replace("\\", "/"))[:-2]))
            fp_vc.write("\n\t\t\t\t\t<FileConfiguration\n\t\t\t\t\t\tName=\"Debug|x64\"\n\t\t\t\t\t\t>\n\t\t\t\t\t\t<Tool\n\t\t\t\t\t\t\tName=\"VCCustomBuildTool\"\n\t\t\t\t\t\t\tCommandLine=\"..\\tools\\flex.exe -i -Pyy_{0} -o$(InputDir)$(InputName).c $(InputPath)&#x0D;&#x0A;\"\n\t\t\t\t\t\t\tOutputs=\"$(InputDir)$(InputName).c\"\n\t\t\t\t\t\t/>\n\t\t\t\t\t</FileConfiguration>".format(os.path.basename(f.replace("\\", "/"))[:-2]))
            fp_vc.write("\n\t\t\t\t\t<FileConfiguration\n\t\t\t\t\t\tName=\"Release|Win32\"\n\t\t\t\t\t\t>\n\t\t\t\t\t\t<Tool\n\t\t\t\t\t\t\tName=\"VCCustomBuildTool\"\n\t\t\t\t\t\t\tCommandLine=\"..\\tools\\flex.exe -i -Pyy_{0} -o$(InputDir)$(InputName).c $(InputPath)&#x0D;&#x0A;\"\n\t\t\t\t\t\t\tOutputs=\"$(InputDir)$(InputName).c\"\n\t\t\t\t\t\t/>\n\t\t\t\t\t</FileConfiguration>".format(os.path.basename(f.replace("\\", "/"))[:-2]))
            fp_vc.write("\n\t\t\t\t\t<FileConfiguration\n\t\t\t\t\t\tName=\"Release|x64\"\n\t\t\t\t\t\t>\n\t\t\t\t\t\t<Tool\n\t\t\t\t\t\t\tName=\"VCCustomBuildTool\"\n\t\t\t\t\t\t\tCommandLine=\"..\\tools\\flex.exe -i -Pyy_{0} -o$(InputDir)$(InputName).c $(InputPath)&#x0D;&#x0A;\"\n\t\t\t\t\t\t\tOutputs=\"$(InputDir)$(InputName).c\"\n\t\t\t\t\t\t/>\n\t\t\t\t\t</FileConfiguration>".format(os.path.basename(f.replace("\\", "/"))[:-2]))
        else:
            fp_vc.write("\n\t\t\t\t\t<FileConfiguration\n\t\t\t\t\t\tName=\"Debug|Win32\"\n\t\t\t\t\t\t>\n\t\t\t\t\t\t<Tool\n\t\t\t\t\t\t\tName=\"VCCLCompilerTool\"\n\t\t\t\t\t\t\tObjectFile=\"$(IntDir)\{0}\\\"\n\t\t\t\t\t\t/>\n\t\t\t\t\t</FileConfiguration>".format(d))
            fp_vc.write("\n\t\t\t\t\t<FileConfiguration\n\t\t\t\t\t\tName=\"Debug|x64\"\n\t\t\t\t\t\t>\n\t\t\t\t\t\t<Tool\n\t\t\t\t\t\t\tName=\"VCCLCompilerTool\"\n\t\t\t\t\t\t\tObjectFile=\"$(IntDir)\{0}\\\"\n\t\t\t\t\t\t/>\n\t\t\t\t\t</FileConfiguration>".format(d))
            fp_vc.write("\n\t\t\t\t\t<FileConfiguration\n\t\t\t\t\t\tName=\"Release|Win32\"\n\t\t\t\t\t\t>\n\t\t\t\t\t\t<Tool\n\t\t\t\t\t\t\tName=\"VCCLCompilerTool\"\n\t\t\t\t\t\t\tObjectFile=\"$(IntDir)\{0}\\\"\n\t\t\t\t\t\t/>\n\t\t\t\t\t</FileConfiguration>".format(d))
            fp_vc.write("\n\t\t\t\t\t<FileConfiguration\n\t\t\t\t\t\tName=\"Release|x64\"\n\t\t\t\t\t\t>\n\t\t\t\t\t\t<Tool\n\t\t\t\t\t\t\tName=\"VCCLCompilerTool\"\n\t\t\t\t\t\t\tObjectFile=\"$(IntDir)\{0}\\\"\n\t\t\t\t\t\t/>\n\t\t\t\t\t</FileConfiguration>".format(d))
            fp_vc.write("\n\t\t\t\t</File>")

    fp_vc.write("\n\t\t\t</Filter>")

fp_vc.write("\n\t\t</Filter>")
fp_vc.close()

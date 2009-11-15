import os.path, sys
from Atrinik import CreatePathname

path = CreatePathname("/python/events/init")
sys.path.insert(0, CreatePathname("/python"))

if os.path.exists(path):
	files = os.listdir(path)

	for file in files:
		if (file.endswith(".py")):
			execfile(os.path.join(path, file))

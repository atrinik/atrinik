import os.path, sys
from Atrinik import *

path = CreatePathname('/python/events/init')

if os.path.exists(path):
	files = os.listdir(path)

	for file in files:
		if (file.endswith('.py')):
			execfile(os.path.join(path, file))

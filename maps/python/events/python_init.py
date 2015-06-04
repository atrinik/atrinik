import os.path, sys, gc
from Atrinik import CreatePathname

path = CreatePathname("/python/events/init")
sys.path.insert(0, CreatePathname("/python"))
gc.enable()

if os.path.exists(path):
    files = os.listdir(path)

    for file in files:
        if file.endswith(".py"):
            with open(os.path.join(path, file)) as f:
                exec(f.read())

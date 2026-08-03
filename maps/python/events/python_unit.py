import tests

if not tests.run():
    raise RuntimeError("Plugin unit tests failed")

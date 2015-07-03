import glob
import os
import unittest

import tests.Atrinik_tests.Atrinik


def run():
    alltests = unittest.TestSuite([
        tests.Atrinik_tests.Atrinik.suite,
    ])

    try:
        import xmlrunner
        path = "tests/unit/plugins/python"

        if os.path.exists(path):
            for name in glob.glob(os.path.join(path, "*.xml")):
                os.unlink(name)

        xmlrunner.XMLTestRunner(output=path).run(alltests)
    except ImportError:
        unittest.TextTestRunner(verbosity=2).run(alltests)

__all__ = ["run"]

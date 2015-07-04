import glob
import os
import unittest

import tests.Atrinik_tests.Atrinik
import tests.Atrinik_tests.Object


def run():
    all_suites = []
    all_suites += tests.Atrinik_tests.Atrinik.suites
    all_suites += tests.Atrinik_tests.Object.suites

    all_tests = unittest.TestSuite(all_suites)

    try:
        import xmlrunner
        path = "tests/unit/plugins/python"

        if os.path.exists(path):
            for name in glob.glob(os.path.join(path, "*.xml")):
                os.unlink(name)

        xmlrunner.XMLTestRunner(output=path).run(all_tests)
    except ImportError:
        unittest.TextTestRunner(verbosity=2).run(all_tests)

__all__ = ["run"]

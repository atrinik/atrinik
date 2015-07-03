import unittest

import tests.Atrinik_tests.Atrinik


def run():
    alltests = unittest.TestSuite([
        tests.Atrinik_tests.Atrinik.suite,
    ])

    try:
        import xmlrunner
        path = "tests/unit/plugins/python"
        xmlrunner.XMLTestRunner(output=path).run(alltests)
    except ImportError:
        unittest.TextTestRunner(verbosity=2).run(alltests)

__all__ = ["run"]

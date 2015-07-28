import glob
import os
import re
import unittest
import time

import Atrinik


def run():
    import tests.Atrinik_tests.Atrinik
    import tests.Atrinik_tests.AttrList
    import tests.Atrinik_tests.Map
    import tests.Atrinik_tests.Object
    import tests.Atrinik_tests.Region

    all_suites = []
    all_suites += tests.Atrinik_tests.Atrinik.suites
    all_suites += tests.Atrinik_tests.AttrList.suites
    all_suites += tests.Atrinik_tests.Map.suites
    all_suites += tests.Atrinik_tests.Object.suites
    all_suites += tests.Atrinik_tests.Region.suites
    old_all_tests = unittest.TestSuite(all_suites)

    unit_test = Atrinik.GetSettings()["plugin_unit_test"]
    if unit_test:
        new_all_suites = []

        for suite in all_suites:
            new_suite = unittest.TestSuite()
            new_all_suites.append(new_suite)
            for test in suite:
                if re.findall(unit_test, test.id(), re.I):
                    new_suite.addTest(test)

        all_suites = new_all_suites

    all_tests = unittest.TestSuite(all_suites)
    if all_tests.countTestCases() == 0:
        suites = {}

        for suite in old_all_tests:
            for test in suite:
                test_parts = test.id().split(".")
                suite_name = test_parts[-2]
                if suite_name not in suites:
                    suites[suite_name] = []
                suites[suite_name].append(test_parts[-1])

        Atrinik.print("Available test cases:")
        for suite in sorted(suites):
            Atrinik.print(" - {}".format(suite))

            for test in sorted(suites[suite]):
                Atrinik.print("   - {}".format(test))

    try:
        import xmlrunner
        path = "tests/unit/plugins/python"

        if os.path.exists(path):
            for name in glob.glob(os.path.join(path, "*.xml")):
                os.unlink(name)

        xmlrunner.XMLTestRunner(output=path).run(all_tests)
    except ImportError:
        unittest.TextTestRunner(verbosity=2).run(all_tests)


def simulate_server(seconds=None, count=None, wait=True, before_cb=None,
                    after_cb=None, **kwargs):
    """
    Simulates the server game loop using :func:`Atrinik.Process`.

    :param seconds: How many seconds to simulate for.
    :type seconds: float
    :param count: How many times to iterate the game loop.
    :type count: int
    :param wait: Whether to wait the correct amount of time after a single
                 iteration (emulating the ticks behavior).
    :type wait: bool
    :param before_cb: Optional function to call before calling
                      :func:`Atrinik.Process`.
    :type before_cb: collections.Callable
    :param after_cb: Optional function to after before calling
                     :func:`Atrinik.Process`.
    :type after_cb: collections.Callable
    :param \**kwargs: Rest of keyword arguments are passed to *before_cb* and
                      *after_cb*.
    :return: How many times the loop was iterated.
    :rtype: int
    """

    assert(count is not None or seconds is not None)

    now = time.time()
    i = 0
    while ((count is not None and i < count) or
           (seconds is not None and time.time() - now < seconds)):
        i += 1

        if before_cb is not None:
            before_cb(**kwargs)

        start = time.time()
        Atrinik.Process()
        end = time.time()

        if after_cb is not None:
            after_cb(**kwargs)

        if not wait:
            continue

        to_wait = (Atrinik.MAX_TIME - (end - start)) / 1000000.0
        if to_wait > 0.0:
            time.sleep(to_wait)

    return i


class TestSuite(unittest.TestCase):
    maxDiff = None

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.obj = None

    def setUp(self):
        simulate_server(count=1, wait=False)

    def field_compare(self, field, val):
        self.assertEqual(getattr(self.obj, field), val)

    def field_test_float(self, field):
        with self.assertRaises(TypeError):
            setattr(self.obj, field, "xxx")

        setattr(self.obj, field, 1.0)
        self.field_compare(field, 1.0)

        setattr(self.obj, field, 1)
        self.field_compare(field, 1.0)

        setattr(self.obj, field, 0.98)
        self.field_compare(field, 0.98)

        setattr(self.obj, field, 0.4238499855555)
        self.field_compare(field, 0.4238499855555)

    def field_test_int(self, field, bits, unsigned=False):
        # Test for non-integer types
        with self.assertRaises(TypeError):
            setattr(self.obj, field, "xxx")

        # Test a negative value with the specified number of bits. Must always
        # overflow for both signed and unsigned.
        with self.assertRaises(OverflowError):
            setattr(self.obj, field, -1 << bits)

        if unsigned:
            # Test negative values for an unsigned int.
            with self.assertRaises(OverflowError):
                setattr(self.obj, field, -1)
            with self.assertRaises(OverflowError):
                setattr(self.obj, field, -1337)

            # Test for overflow with one over the maximum limit.
            with self.assertRaises(OverflowError):
                setattr(self.obj, field, 1 << bits)
        else:
            # Test signed overflow with one over the maximum limit.
            with self.assertRaises(OverflowError):
                setattr(self.obj, field, (1 << (bits - 1)))

            # Test signed overflow with one over the minimum limit.
            with self.assertRaises(OverflowError):
                setattr(self.obj, field, (-1 << (bits - 1)) - 1)

        # Test for large overflows, that wouldn't even fit in 64-bit numbers.
        with self.assertRaises(OverflowError):
            setattr(self.obj, field, 1 << 256)
        with self.assertRaises(OverflowError):
            setattr(self.obj, field, -1 << 256)

        # Test that zero can be set.
        setattr(self.obj, field, 0)
        self.field_compare(field, 0)

        # Test some numbers.
        setattr(self.obj, field, 1)
        self.field_compare(field, 1)
        setattr(self.obj, field, 42)
        self.field_compare(field, 42)

        if unsigned:
            # Test that maximum can be set.
            setattr(self.obj, field, (1 << bits) - 1)
            self.field_compare(field, (1 << bits) - 1)
        else:
            # Test that minimum can be set.
            setattr(self.obj, field, (-1 << (bits - 1)))
            self.field_compare(field, (-1 << (bits - 1)))
            # Test that maximum can be set.
            setattr(self.obj, field, (1 << (bits - 1)) - 1)
            self.field_compare(field, (1 << (bits - 1)) - 1)

    def flag_compare(self, flag, val):
        self.assertEqual(getattr(self.obj, flag), val)

    def flag_test(self, flag):
        with self.assertRaises(TypeError):
            setattr(self.obj, flag, "xxx")
        with self.assertRaises(TypeError):
            setattr(self.obj, flag, 1)
        with self.assertRaises(TypeError):
            setattr(self.obj, flag, 0)

        setattr(self.obj, flag, True)
        self.flag_compare(flag, True)
        setattr(self.obj, flag, False)
        self.flag_compare(flag, False)
        setattr(self.obj, flag, True)
        self.flag_compare(flag, True)

__all__ = ["run", "simulate_server"]

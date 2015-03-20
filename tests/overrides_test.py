import unittest
from gi.repository import BlockDev

class OverridesTestCase(unittest.TestCase):
    def test_error_proxy(self):
        """Verify that the error proxy works as expected"""

        # calls via the error proxy has to be done as
        # e.g. BlockDev.swap.swapon() instead of BlockDev.swap_swapon(), since
        # BlockDev.swap is an ErrorProxy instance and BlockDev.swap_swapon() is
        # the function it calls

        # test that exceptions are correctly transformed
        try:
            # no need to specify priority since we are using overrides that
            # define the default value for the parameter (-1)
            BlockDev.swap.swapon("/non/existing")
        except BlockDev.BlockDevError as e:
            # we caught the generic error, now let's test that it is also the
            # fine-grained one
            self.assertTrue(isinstance(e, BlockDev.SwapError))

        # test that a second call like that works the same (should go from the cache)
        try:
            BlockDev.swap.swapon("/non/existing")
        except BlockDev.BlockDevError as e:
            self.assertTrue(isinstance(e, BlockDev.SwapError))

        # test that successful calls propagate the results
        self.assertTrue(BlockDev.lvm.is_supported_pe_size(4 * 1024))
        self.assertEqual(BlockDev.lvm.round_size_to_pe(11 * 1024**2, 4 * 1024**2, True), 12 * 1024**2)


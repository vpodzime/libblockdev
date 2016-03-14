import unittest
import os
from utils import create_sparse_tempfile
import overrides_hack

from gi.repository import BlockDev, GLib
if not BlockDev.is_initialized():
    BlockDev.init(None, None)

class PartTestCase(unittest.TestCase):
    def setUp(self):
        self.addCleanup(self._clean_up)
        self.dev_file = create_sparse_tempfile("part_test", 1024**3)
        self.dev_file2 = create_sparse_tempfile("part_test", 1024**3)
        succ, loop = BlockDev.loop_setup(self.dev_file)
        if not succ:
            raise RuntimeError("Failed to setup loop device for testing")
        self.loop_dev = "/dev/%s" % loop
        succ, loop = BlockDev.loop_setup(self.dev_file2)
        if not succ:
            raise RuntimeError("Failed to setup loop device for testing")
        self.loop_dev2 = "/dev/%s" % loop

    def _clean_up(self):
        succ = BlockDev.loop_teardown(self.loop_dev)
        if not succ:
            os.unlink(self.dev_file)
            raise RuntimeError("Failed to tear down loop device used for testing")
        os.unlink(self.dev_file)
        succ = BlockDev.loop_teardown(self.loop_dev2)
        if  not succ:
            os.unlink(self.dev_file2)
            raise RuntimeError("Failed to tear down loop device used for testing")
        os.unlink(self.dev_file2)

class PartCreateTableCase(PartTestCase):
    def test_create_table(self):
        """Verify that it is possible to create a new partition table"""

        # doesn't matter if we want to ignore any preexisting partition tables
        # or not on a nonexisting device
        with self.assertRaises(GLib.GError):
            BlockDev.part_create_table ("/non/existing", BlockDev.PartTableType.MSDOS, False)

        with self.assertRaises(GLib.GError):
            BlockDev.part_create_table ("/non/existing", BlockDev.PartTableType.MSDOS, True)

        # doesn't matter if we want to ignore any preexisting partition tables
        # or not on a clean device
        succ = BlockDev.part_create_table (self.loop_dev, BlockDev.PartTableType.MSDOS, False)
        self.assertTrue(succ)

        succ = BlockDev.part_create_table (self.loop_dev2, BlockDev.PartTableType.MSDOS, True)
        self.assertTrue(succ)

        # should fail because of a preexisting partition table (and not ignoring it)
        with self.assertRaises(GLib.GError):
            BlockDev.part_create_table (self.loop_dev, BlockDev.PartTableType.MSDOS, False)

        # should succeed if we want to ignore any preexisting partition tables
        succ = BlockDev.part_create_table (self.loop_dev, BlockDev.PartTableType.MSDOS, True)
        self.assertTrue(succ)

        succ = BlockDev.part_create_table (self.loop_dev, BlockDev.PartTableType.GPT, True)
        self.assertTrue(succ)

class PartSetFlagCase(PartTestCase):
    def test_set_part_flag(self):
        """Verify that it is possible to set a partition flag"""
        # TODO: create partition, set flag, check result, unset flag, check result
        pass

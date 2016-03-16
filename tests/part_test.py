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
        self.dev_file = create_sparse_tempfile("part_test", 100 * 1024**2)
        self.dev_file2 = create_sparse_tempfile("part_test", 100 * 1024**2)
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

class PartCreatePartCase(PartTestCase):
    def test_create_part_simple(self):
        """Verify that it is possible to create a parition"""

        # we first need a partition table
        succ = BlockDev.part_create_table (self.loop_dev, BlockDev.PartTableType.MSDOS, True)
        self.assertTrue(succ)

        # for now, let's just create a typical primary partition starting at the
        # sector 2048, 10 MiB big with optimal alignment
        ps = BlockDev.part_create_part (self.loop_dev, BlockDev.PartTypeReq.NORMAL, 2048*512, 10 * 1024**2, BlockDev.PartAlign.OPTIMAL)

        # we should get proper data back
        self.assertTrue(ps)
        self.assertEqual(ps.path, self.loop_dev + "p1")
        self.assertEqual(ps.type, BlockDev.PartType.NORMAL)
        self.assertEqual(ps.start, 2048 * 512)
        self.assertEqual(ps.size, 10 * 1024**2)
        self.assertEqual(ps.flags, 0)  # no flags (combination of bit flags)

        ps2 = BlockDev.part_get_part_spec (self.loop_dev, ps.path)
        self.assertEqual(ps.path, ps2.path)
        self.assertEqual(ps.type, ps2.type)
        self.assertEqual(ps.start, ps2.start)
        self.assertEqual(ps.size, ps2.size)
        self.assertEqual(ps.flags, ps2.flags)

        pss = BlockDev.part_get_disk_parts (self.loop_dev)
        self.assertEqual(len(pss), 1)
        ps3 = pss[0]
        self.assertEqual(ps.path, ps3.path)
        self.assertEqual(ps.type, ps3.type)
        self.assertEqual(ps.start, ps3.start)
        self.assertEqual(ps.size, ps3.size)
        self.assertEqual(ps.flags, ps3.flags)

class PartCreateDeletePartCase(PartTestCase):
    def test_create_delete_part_simple(self):
        """Verify that it is possible to create and delete a parition"""

        # we first need a partition table
        succ = BlockDev.part_create_table (self.loop_dev, BlockDev.PartTableType.MSDOS, True)
        self.assertTrue(succ)

        # for now, let's just create a typical primary partition starting at the
        # sector 2048, 10 MiB big with optimal alignment
        ps = BlockDev.part_create_part (self.loop_dev, BlockDev.PartTypeReq.NORMAL, 2048*512, 10 * 1024**2, BlockDev.PartAlign.OPTIMAL)
        pss = BlockDev.part_get_disk_parts (self.loop_dev)
        self.assertEqual(len(pss), 1)

        succ = BlockDev.part_delete_part (self.loop_dev, ps.path)
        self.assertTrue(succ)
        pss = BlockDev.part_get_disk_parts (self.loop_dev)
        self.assertEqual(len(pss), 0)

class PartSetFlagCase(PartTestCase):
    def test_set_part_flag(self):
        """Verify that it is possible to set a partition flag"""

        # we first need a partition table
        succ = BlockDev.part_create_table (self.loop_dev, BlockDev.PartTableType.MSDOS, True)
        self.assertTrue(succ)

        # for now, let's just create a typical primary partition starting at the
        # sector 2048, 10 MiB big with optimal alignment
        ps = BlockDev.part_create_part (self.loop_dev, BlockDev.PartTypeReq.NORMAL, 2048*512, 10 * 1024**2, BlockDev.PartAlign.OPTIMAL)

        # we should get proper data back
        self.assertTrue(ps)
        self.assertEqual(ps.flags, 0)  # no flags (combination of bit flags)

        succ = BlockDev.part_set_part_flag (self.loop_dev, ps.path, BlockDev.PartFlag.BOOT, True)
        self.assertTrue(succ)
        ps = BlockDev.part_get_part_spec (self.loop_dev, ps.path)
        self.assertTrue(ps.flags & BlockDev.PartFlag.BOOT)

        succ = BlockDev.part_set_part_flag (self.loop_dev, ps.path, BlockDev.PartFlag.BOOT, False)
        self.assertTrue(succ)
        ps = BlockDev.part_get_part_spec (self.loop_dev, ps.path)
        self.assertFalse(ps.flags & BlockDev.PartFlag.BOOT)

        # add another partition and do some more tests on that one
        ps = BlockDev.part_create_part (self.loop_dev, BlockDev.PartTypeReq.NORMAL, ps.start + ps.size + 1, 10 * 1024**2, BlockDev.PartAlign.OPTIMAL)

        succ = BlockDev.part_set_part_flag (self.loop_dev, ps.path, BlockDev.PartFlag.BOOT, True)
        self.assertTrue(succ)
        succ = BlockDev.part_set_part_flag (self.loop_dev, ps.path, BlockDev.PartFlag.LVM, True)
        self.assertTrue(succ)
        ps = BlockDev.part_get_part_spec (self.loop_dev, ps.path)
        self.assertTrue(ps.flags & BlockDev.PartFlag.BOOT)
        self.assertTrue(ps.flags & BlockDev.PartFlag.LVM)

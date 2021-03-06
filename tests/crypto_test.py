import unittest
import os
import tempfile
import overrides_hack
import shutil
import subprocess
import six

from utils import create_sparse_tempfile
from gi.repository import BlockDev, GLib
if not BlockDev.is_initialized():
    BlockDev.init(None, None)

PASSWD = "myshinylittlepassword"
PASSWD2 = "myshinylittlepassword2"
PASSWD3 = "myshinylittlepassword3"

class CryptoTestGenerateBackupPassphrase(unittest.TestCase):
    def test_generate_backup_passhprase(self):
        """Verify that backup passphrase generation works as expected"""

        exp = r"^([0-9A-Za-z./]{5}-){3}[0-9A-Za-z./]{5}$"
        for _i in range(100):
            bp = BlockDev.crypto_generate_backup_passphrase()
            six.assertRegex(self, bp, exp)

class CryptoTestCase(unittest.TestCase):
    def setUp(self):
        self.dev_file = create_sparse_tempfile("crypto_test", 1024**3)
        self.dev_file2 = create_sparse_tempfile("crypto_test2", 1024**3)
        succ, loop = BlockDev.loop_setup(self.dev_file)
        if not succ:
            raise RuntimeError("Failed to setup loop device for testing")
        self.loop_dev = "/dev/%s" % loop

        succ, loop = BlockDev.loop_setup(self.dev_file2)
        if not succ:
            raise RuntimeError("Failed to setup loop device for testing")
        self.loop_dev2 = "/dev/%s" % loop

        # make a key file
        handle, self.keyfile = tempfile.mkstemp(prefix="libblockdev_test_keyfile", text=False)
        os.write(handle, b"nobodyknows")
        os.close(handle)

    def tearDown(self):
        try:
            BlockDev.crypto_luks_close("libblockdevTestLUKS")
        except:
            pass

        succ = BlockDev.loop_teardown(self.loop_dev)
        if not succ:
            os.unlink(self.dev_file)
            raise RuntimeError("Failed to tear down loop device used for testing")

        os.unlink(self.dev_file)

        succ = BlockDev.loop_teardown(self.loop_dev2)
        if not succ:
            os.unlink(self.dev_file2)
            raise RuntimeError("Failed to tear down loop device used for testing")

        os.unlink(self.dev_file2)
        os.unlink(self.keyfile)

class CryptoTestFormat(CryptoTestCase):
    @unittest.skipIf("SKIP_SLOW" in os.environ, "skipping slow tests")
    def test_format(self):
        """Verify that formating device as LUKS works"""

        # no passphrase nor keyfile
        with self.assertRaises(GLib.GError):
            BlockDev.crypto_luks_format(self.loop_dev, None, 0, None, None, 0)

        # the simple case with password
        succ = BlockDev.crypto_luks_format(self.loop_dev, "aes-cbc-essiv:sha256", 0, PASSWD, None, 0)
        self.assertTrue(succ)

        # create with a keyfile
        succ = BlockDev.crypto_luks_format(self.loop_dev, "aes-cbc-essiv:sha256", 0, None, self.keyfile, 0)
        self.assertTrue(succ)

class CryptoTestResize(CryptoTestCase):
    @unittest.skipIf("SKIP_SLOW" in os.environ, "skipping slow tests")
    def test_resize(self):
        """Verify that resizing LUKS device works"""

        # the simple case with password
        succ = BlockDev.crypto_luks_format(self.loop_dev, "aes-cbc-essiv:sha256", 0, PASSWD, None, 0)
        self.assertTrue(succ)

        succ = BlockDev.crypto_luks_open(self.loop_dev, "libblockdevTestLUKS", PASSWD, None)
        self.assertTrue(succ)

        # resize to 512 KiB (1024 * 512B sectors)
        succ = BlockDev.crypto_luks_resize("libblockdevTestLUKS", 1024)
        self.assertTrue(succ)

        # resize back to full size
        succ = BlockDev.crypto_luks_resize("libblockdevTestLUKS", 0)
        self.assertTrue(succ)

        succ = BlockDev.crypto_luks_close("libblockdevTestLUKS")
        self.assertTrue(succ)

class CryptoTestOpenClose(CryptoTestCase):
    @unittest.skipIf("SKIP_SLOW" in os.environ, "skipping slow tests")
    def test_luks_open_close(self):
        """Verify that opening/closing LUKS device works"""

        succ = BlockDev.crypto_luks_format(self.loop_dev, None, 0, PASSWD, self.keyfile, 0)
        self.assertTrue(succ)

        with self.assertRaises(GLib.GError):
            BlockDev.crypto_luks_open("/non/existing/device", "libblockdevTestLUKS", PASSWD, None)

        with self.assertRaises(GLib.GError):
            BlockDev.crypto_luks_open(self.loop_dev, "libblockdevTestLUKS", None, None)

        with self.assertRaises(GLib.GError):
            BlockDev.crypto_luks_open(self.loop_dev, "libblockdevTestLUKS", "wrong-passhprase", None)

        with self.assertRaises(GLib.GError):
            BlockDev.crypto_luks_open(self.loop_dev, "libblockdevTestLUKS", None, "wrong-keyfile")

        succ = BlockDev.crypto_luks_open(self.loop_dev, "libblockdevTestLUKS", PASSWD, None)
        self.assertTrue(succ)

        # use the full /dev/mapper/ path
        succ = BlockDev.crypto_luks_close("/dev/mapper/libblockdevTestLUKS")
        self.assertTrue(succ)

        succ = BlockDev.crypto_luks_open(self.loop_dev, "libblockdevTestLUKS", None, self.keyfile)
        self.assertTrue(succ)

        # use just the LUKS device name
        succ = BlockDev.crypto_luks_close("libblockdevTestLUKS")
        self.assertTrue(succ)

class CryptoTestAddKey(CryptoTestCase):
    @unittest.skipIf("SKIP_SLOW" in os.environ, "skipping slow tests")
    def test_add_key(self):
        """Verify that adding key to LUKS device works"""

        succ = BlockDev.crypto_luks_format(self.loop_dev, None, 0, PASSWD, None, 0)
        self.assertTrue(succ)

        with self.assertRaises(GLib.GError):
            BlockDev.crypto_luks_add_key(self.loop_dev, "wrong-passphrase", None, PASSWD2, None)

        succ = BlockDev.crypto_luks_add_key(self.loop_dev, PASSWD, None, PASSWD2, None)
        self.assertTrue(succ)

class CryptoTestRemoveKey(CryptoTestCase):
    @unittest.skipIf("SKIP_SLOW" in os.environ, "skipping slow tests")
    def test_remove_key(self):
        """Verify that removing key from LUKS device works"""

        succ = BlockDev.crypto_luks_format(self.loop_dev, None, 0, PASSWD, None, 0)
        self.assertTrue(succ)

        succ = BlockDev.crypto_luks_add_key(self.loop_dev, PASSWD, None, PASSWD2, None)
        self.assertTrue(succ)

        with self.assertRaises(GLib.GError):
            BlockDev.crypto_luks_remove_key(self.loop_dev, "wrong-passphrase", None)

        succ = BlockDev.crypto_luks_remove_key(self.loop_dev, PASSWD, None)
        self.assertTrue(succ)

class CryptoTestChangeKey(CryptoTestCase):
    @unittest.skipIf("SKIP_SLOW" in os.environ, "skipping slow tests")
    def test_change_key(self):
        """Verify that changing key in LUKS device works"""

        succ = BlockDev.crypto_luks_format(self.loop_dev, None, 0, PASSWD, None, 0)
        self.assertTrue(succ)

        succ = BlockDev.crypto_luks_change_key(self.loop_dev, PASSWD, PASSWD2)
        self.assertTrue(succ)

class CryptoTestIsLuks(CryptoTestCase):
    @unittest.skipIf("SKIP_SLOW" in os.environ, "skipping slow tests")
    def test_is_luks(self):
        """Verify that LUKS device recognition works"""

        with self.assertRaises(GLib.GError):
            BlockDev.crypto_device_is_luks("/non/existing/device")

        succ = BlockDev.crypto_luks_format(self.loop_dev, None, 0, PASSWD, None, 0)
        self.assertTrue(succ)

        is_luks = BlockDev.crypto_device_is_luks(self.loop_dev)
        self.assertTrue(is_luks)

        is_luks = BlockDev.crypto_device_is_luks(self.loop_dev2)
        self.assertFalse(is_luks)

class CryptoTestLuksStatus(CryptoTestCase):
    @unittest.skipIf("SKIP_SLOW" in os.environ, "skipping slow tests")
    def test_luks_status(self):
        """Verify that LUKS device status reporting works"""

        with self.assertRaises(GLib.GError):
            BlockDev.crypto_luks_status("/non/existing/device")

        succ = BlockDev.crypto_luks_format(self.loop_dev, None, 0, PASSWD, None, 0)
        self.assertTrue(succ)

        succ = BlockDev.crypto_luks_open(self.loop_dev, "libblockdevTestLUKS", PASSWD, None)
        self.assertTrue(succ)

        # use the full /dev/mapper path
        status = BlockDev.crypto_luks_status("/dev/mapper/libblockdevTestLUKS")
        self.assertEqual(status, "active")

        # use just the LUKS device name
        status = BlockDev.crypto_luks_status("libblockdevTestLUKS")
        self.assertEqual(status, "active")

        succ = BlockDev.crypto_luks_close("libblockdevTestLUKS")
        self.assertTrue(succ)

        with self.assertRaises(GLib.GError):
            BlockDev.crypto_luks_status("libblockdevTestLUKS")

class CryptoTestGetUUID(CryptoTestCase):
    @unittest.skipIf("SKIP_SLOW" in os.environ, "skipping slow tests")
    def test_get_uuid(self):
        """Verify that getting LUKS device UUID works"""

        succ = BlockDev.crypto_luks_format(self.loop_dev, None, 0, PASSWD, None, 0)
        self.assertTrue(succ)

        uuid = BlockDev.crypto_luks_uuid(self.loop_dev)
        self.assertTrue(uuid)

        with self.assertRaises(GLib.GError):
            uuid = BlockDev.crypto_luks_uuid(self.loop_dev2)

class CryptoTestLuksOpenRW(CryptoTestCase):
    @unittest.skipIf("SKIP_SLOW" in os.environ, "skipping slow tests")
    def test_luks_open_rw(self):
        """Verify that opened LUKS device is usable (activated as RW)"""

        succ = BlockDev.crypto_luks_format(self.loop_dev, None, 0, PASSWD, None, 0)
        self.assertTrue(succ)

        succ = BlockDev.crypto_luks_open(self.loop_dev, "libblockdevTestLUKS", PASSWD, None)
        self.assertTrue(succ)

        # tests that we can write something to the raw LUKS device
        succ = BlockDev.utils_exec_and_report_error(["dd", "if=/dev/zero", "of=/dev/mapper/libblockdevTestLUKS", "bs=1M", "count=1"])
        self.assertTrue(succ)

        succ = BlockDev.crypto_luks_close("libblockdevTestLUKS")
        self.assertTrue(succ)

class CryptoTestEscrow(CryptoTestCase):
    def setUp(self):
        super(CryptoTestEscrow, self).setUp()

        # Create the certificate used to encrypt the escrow packet and backup passphrase.
        # volume_key requires a nss database directory to decrypt any of the
        # packet files, and python-nss is python2 only, just do everything with
        # shell commands.

        self.nss_dir = tempfile.mkdtemp(prefix='libblockdev_test_escrow')
        self.addCleanup(shutil.rmtree, self.nss_dir)
        subprocess.check_call(['certutil', '-d', self.nss_dir, '--empty-password', '-N'])

        # Gather some entropy to keep certutil from asking for input
        with tempfile.NamedTemporaryFile() as noise_file:
            noise_file.write(os.urandom(20))
            noise_file.flush()

            subprocess.check_call(['certutil', '-d', self.nss_dir, '-S', '-x', '-n',
                'escrow_cert', '-s', 'CN=Escrow Test', '-t', ',,TC', '-z',
                noise_file.name])

        # Export the public certificate
        handle, self.public_cert = tempfile.mkstemp(prefix='libblockdev_test_escrow')

        os.close(handle)
        subprocess.check_call(['certutil', '-d', self.nss_dir, '-L', '-n', 'escrow_cert',
            '-a', '-o', self.public_cert])
        self.addCleanup(os.unlink, self.public_cert)

    @unittest.skipIf("SKIP_SLOW" in os.environ, "skipping slow tests")
    def test_escrow_packet(self):
        """Verify that an escrow packet can be created for a device"""

        succ = BlockDev.crypto_luks_format(self.loop_dev, None, 0, PASSWD, None, 0)
        self.assertTrue(succ)

        escrow_dir = tempfile.mkdtemp(prefix='libblockdev_test_escrow')
        self.addCleanup(shutil.rmtree, escrow_dir)
        with open(self.public_cert, 'rb') as cert_file:
            succ = BlockDev.crypto_escrow_device(self.loop_dev, PASSWD, cert_file.read(),
                    escrow_dir, None)
        self.assertTrue(succ)

        # Find the escrow packet
        escrow_packet_file = '%s/%s-escrow' % (escrow_dir, BlockDev.crypto_luks_uuid(self.loop_dev))
        self.assertTrue(os.path.isfile(escrow_packet_file))

        # Use the volume_key utility (see note in setUp about why not python)
        # to decrypt the escrow packet and restore access to the volume under
        # a new passphrase

        # Just use the existing temp directory to output the re-encrypted packet
        # PASSWD2 is the passphrase of the new escrow packet
        p = subprocess.Popen(['volume_key', '--reencrypt', '-b', '-d', self.nss_dir,
            escrow_packet_file, '-o', '%s/escrow-out' % escrow_dir],
            stdin=subprocess.PIPE)
        p.communicate(input=('%s\0%s\0' % (PASSWD2, PASSWD2)).encode('utf-8'))
        if p.returncode != 0:
            raise subprocess.CalledProcessError(p.returncode, 'volume_key')

        # Restore access to the volume
        # PASSWD3 is the new passphrase for the LUKS device
        p = subprocess.Popen(['volume_key', '--restore', '-b', self.loop_dev,
            '%s/escrow-out' % escrow_dir], stdin=subprocess.PIPE)
        p.communicate(input=('%s\0%s\0%s\0' % (PASSWD2, PASSWD3, PASSWD3)).encode('utf-8'))
        if p.returncode != 0:
            raise subprocess.CalledProcessError(p.returncode, 'volume_key')

        # Open the volume with the new passphrase
        succ = BlockDev.crypto_luks_open(self.loop_dev, 'libblockdevTestLUKS', PASSWD3, None)
        self.assertTrue(succ)

    @unittest.skipIf("SKIP_SLOW" in os.environ, "skipping slow tests")
    def test_backup_passphrase(self):
        """Verify that a backup passphrase can be created for a device"""
        succ = BlockDev.crypto_luks_format(self.loop_dev, None, 0, PASSWD, None, 0)
        self.assertTrue(succ)

        escrow_dir = tempfile.mkdtemp(prefix='libblockdev_test_escrow')
        self.addCleanup(shutil.rmtree, escrow_dir)
        backup_passphrase = BlockDev.crypto_generate_backup_passphrase()
        with open(self.public_cert, 'rb') as cert_file:
            succ = BlockDev.crypto_escrow_device(self.loop_dev, PASSWD, cert_file.read(),
                    escrow_dir, backup_passphrase)
        self.assertTrue(succ)

        # Find the backup passphrase
        escrow_backup_passphrase = "%s/%s-escrow-backup-passphrase" % (escrow_dir, BlockDev.crypto_luks_uuid(self.loop_dev))
        self.assertTrue(os.path.isfile(escrow_backup_passphrase))

        # Check that the encrypted file contains what we put in
        env = os.environ
        env.update({"LC_ALL": "C"})
        passphrase = subprocess.check_output(
                ['volume_key', '--secrets', '-d', self.nss_dir, escrow_backup_passphrase],
                env=env)
        passphrase = passphrase.strip().split()[1].decode('ascii')
        self.assertEqual(passphrase, backup_passphrase)

        # Check that the backup passphrase works
        succ = BlockDev.crypto_luks_open(self.loop_dev, 'libblockdevTestLUKS', backup_passphrase, None)
        self.assertTrue(succ)

import threading
import sys
import time

from gi.repository import BlockDev
BlockDev.init()

class ProgReporter(object):
    def __init__(self):
        self._lock = threading.Lock()
        self._thread_actions = dict()
        self._tasks_threads = dict()
        self._progress = dict()

    def add_task(self, thread, action_desc):
        with self._lock:
            self._thread_actions[thread] = action_desc

    def process_progress(self, task_id, status, completion, msg):
        if status == BlockDev.UtilsProgStatus.STARTED:
            with self._lock:
                self._tasks_threads[task_id] = threading.current_thread()
                self._progress[task_id] = 0
            print(msg)
        elif status == BlockDev.UtilsProgStatus.PROGRESS:
            if self._progress[task_id] != completion:
                print("%s: %d %%" % (self._thread_actions[self._tasks_threads[task_id]], completion))
                self._progress[task_id] = completion
        else:
            print("%s: %s" % (self._thread_actions[self._tasks_threads[task_id]], msg))

prog_reporter = ProgReporter()
BlockDev.utils_init_prog_reporting(prog_reporter.process_progress)

def run_pvmove(src, dst=None, wait=0):
    action = "pvmove %s" % src
    if dst:
        action += " %s" % dst
    prog_reporter.add_task(threading.current_thread(), action)
    time.sleep(wait)
    BlockDev.lvm.pvmove(src, dst)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        exit(1)

    print("Creating threads")
    if sys.argv[1] == "ab":
        thread1 = threading.Thread(target=run_pvmove, args=("/dev/sda1", "/dev/sdb1"))
        thread2 = threading.Thread(target=run_pvmove, args=("/dev/sda2", "/dev/sdb2", 5))
    else:
        thread1 = threading.Thread(target=run_pvmove, args=("/dev/sdb1", "/dev/sda1"))
        thread2 = threading.Thread(target=run_pvmove, args=("/dev/sdb2", "/dev/sda2"))


    print("Starting threads")
    thread1.start()
    thread2.start()

    print("Waiting for threads")
    thread1.join()
    thread2.join()
    print("Done")

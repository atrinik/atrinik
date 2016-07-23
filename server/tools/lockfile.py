import sys, os, signal, time, atexit, signal

have_fcntl = False

try:
    import fcntl
    have_fcntl = True
except ImportError:
    pass

class LockError(Exception):
    pass

class LockFile(object):
    def __init__(self, file_path):
        base = os.path.basename(file_path).replace(".py", ".pid")
        self.path = os.path.join(os.path.dirname(file_path), base)

        try:
            with open(self.path) as f:
                self.old_pid = int(f.read().strip())
        except IOError:
            self.old_pid = None

        signal.signal(signal.SIGTERM, self.cleanup)

    def lock(self):
        if have_fcntl:
            self.pid_file = open(self.path, "w")

            try:
                fcntl.lockf(self.pid_file, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except IOError:
                self.pid_file.close()
                raise LockError()
        else:
            if os.path.exists(self.path):
                try:
                    os.unlink(self.path)
                except IOError:
                    raise LockError()

            self.pid_file = open(self.path, "w")

        self.pid_file.write("%d\n" % os.getpid())
        self.pid_file.flush()

    def cleanup(self, *args):
        sys.exit(0)

    def __enter__(self):
        if self.old_pid is None:
            self.lock()
            return

        try:
            self.lock()
        except LockError:
            # Ask politely first.
            os.kill(self.old_pid, signal.SIGTERM)
            time.sleep(0.5)

            try:
                self.lock()
            except LockError:
                os.kill(self.old_pid, signal.SIGKILL)
                time.sleep(0.5)
                self.lock()

    def __exit__(self, *args):
        try:
            self.lock()
            os.unlink(self.path)
        except LockError:
            pass

        self.pid_file.close()

import fcntl, sys, os, signal, time, atexit, signal

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
        self.pid_file = open(self.path, "w")

        try:
            fcntl.lockf(self.pid_file, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except IOError:
            self.pid_file.close()
            raise

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
        except IOError:
            # Ask politely first.
            os.kill(self.old_pid, signal.SIGTERM)
            time.sleep(0.5)

            try:
                self.lock()
            except IOError:
                os.kill(self.old_pid, signal.SIGKILL)
                time.sleep(0.5)
                self.lock()

    def __exit__(self, *args):
        try:
            self.lock()
            os.unlink(self.path)
        except IOError:
            pass

        self.pid_file.close()

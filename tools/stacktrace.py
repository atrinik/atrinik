#!/usr/bin/python
import sys, re, subprocess

def main ():
    if len(sys.argv) < 3:
        print("Usage: ./stacktrace.py <executable> <stacktrace file>")
        return

    with open(sys.argv[2], "r") as f:
        for line in f:
            match = re.match(r"\d+:\s*(.+)\s*", line, re.I)

            if not match:
                continue

            process = subprocess.Popen(["addr2line", "-e", sys.argv[1], "-f",
                    "-p", match.group(1)], stdout = subprocess.PIPE)
            (output, err) = process.communicate()
            exit_code = process.wait()

            print(output.strip().decode("ascii"))

if __name__ == "__main__":
    main()

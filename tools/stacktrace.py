#!/usr/bin/python
import sys, re, subprocess

def main ():
    if len(sys.argv) < 3:
        print("Usage: ./stacktrace.py <executable> <stacktrace file>")
        return

    with open(sys.argv[2], "r") as f:
        for line in f:
            line = line.strip()
            address = None

            if line.endswith("]"):
                address = line.split()[-1][1:-1]
            else:
	            match = re.match(r"\d+:\s*(.+)\s*", line, re.I)

	            if match:
					address = match.group(1)

            if address == None:
                print(line)
                continue

            process = subprocess.Popen(["addr2line", "-e", sys.argv[1], "-f",
                    "-p", address], stdout = subprocess.PIPE)
            (output, err) = process.communicate()
            exit_code = process.wait()

            print(output.strip().decode("ascii"))

if __name__ == "__main__":
    main()

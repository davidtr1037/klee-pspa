import subprocess


def run_klee(args):
    base_args = [
        "klee",
        "-libc=uclibc",
    ]
    command = base_args + args
    p = subprocess.Popen(command)
    p.communicate()
    return p.returncode

def parse_command_file(path):
    with open(path) as f:
        line = f.readline()
        return line.split()

def get_targets(path):
    with open(path) as f:
        lines = [line.strip() for line in f.readlines()]
        targets = [line for line in lines if line != "" and not line.startswith("#")]
        return targets


#!/usr/bin/env python

import optparse
import os


class Analysis(object):

    def __init__(self):
        self.points_to = {}

    def update(self, function, value, pts):
        if function not in self.points_to:
            self.points_to[function] = {}

        self.points_to[function][value] = pts

    def dump(self):
        for f, m in self.points_to.iteritems():
            print "Function: %s" % f
            for value, pts in m.iteritems():
                print "%d: %r" % (value, pts, )


class PTALogParser(object):
    BANNER = "----"

    def __init__(self, path):
        self.path = path
        self.analyses = []
        self.current = None

    def parse(self):
        with open(self.path) as f:
            for line in f.readlines():
                self.parse_line(line.strip())

    def parse_line(self, line):
        if line == self.BANNER:
            self.current = Analysis()
            self.analyses.append(self.current)
            return

        tokens = line.split()
        function, value = tokens[:2]
        pts = tokens[2:]
        self.add_result(function, int(value), map(int, pts))

    def add_result(self, function, value, pts):
        self.current.update(function, value, pts)

    def dump(self):
        for a in self.analyses:
            a.dump()


def main():
    p = optparse.OptionParser()
    opts, args = p.parse_args()
    if len(args) != 1:
        print "Usage: <pta_log>"
        return 1

    log_file, = args
    if not os.path.exists(log_file):
        print "File not found..."
        return 1

    parser = PTALogParser(log_file)
    parser.parse()
    parser.dump()

if __name__ == '__main__':
    main()


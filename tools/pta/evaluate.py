#!/usr/bin/env python

import optparse
import os
import hashlib


class Analysis(object):

    def __init__(self):
        self.points_to = {}
        self.hashes = {}

    def update(self, function, value, pts):
        if function not in self.points_to:
            self.points_to[function] = {}

        self.points_to[function][value] = pts

    def compute_hash(self):
        for f, m in self.points_to.iteritems():
            h = hashlib.md5()
            for value, pts in m.iteritems():
                h.update("%d %r" % (value, pts, ))
            self.hashes[f] = int(h.hexdigest(), 16)

    def dump(self):
        for f, m in self.points_to.iteritems():
            print "Function: %s" % f
            for value, pts in m.iteritems():
                print "%d: %r" % (value, pts, )

    def __eq__(self, other):
        if len(self.hashes.keys()) != len(other.hashes.keys()):
            return False

        for f, h in self.hashes.iteritems():
            if f not in other.hashes:
                return False

            if h != other.hashes[f]:
                return False

        return True

    def __ne__(self, other):
        return not self == other


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

        for a in self.analyses:
            a.compute_hash()

    def parse_line(self, line):
        if line == self.BANNER:
            self.current = Analysis()
            self.analyses.append(self.current)
            return

        tokens = line.split()
        function, value = tokens[:2]
        pts = tokens[2:]
        self.add_result(function, int(value), sorted(map(int, pts)))

    def add_result(self, function, value, pts):
        self.current.update(function, value, pts)

    def get_results_count(self):
        return len(self.analyses)

    def get_unique_results_count(self):
        return len(self.find_unique())

    def find_unique(self):
        unique = []
        for a in set(self.analyses):
            if a not in unique:
                unique.append(a)

        return unique

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
    print "%d / %d" % (parser.get_unique_results_count(), parser.get_results_count(), )

if __name__ == '__main__':
    main()


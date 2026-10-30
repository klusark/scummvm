#!/usr/bin/env python3
"""Compares the frames that several runs of Grim Fandango wrote with --dump-frames, one run for each renderer
(--renderer=opengl_shaders, opengl, tinygl), all of them with the same --sim-fps and the same --autorun script.

    framediff.py [options] <frames dir of the reference> <frames dir> [<frames dir>...] -o <output dir>

The first directory is the reference, every other one is compared with it, frame by frame. A frame is listed when a pixel of it
differs by more than --threshold in any colour channel (0 = any difference); for those the output directory gets
<reference>_<frame>.png, <name>_<frame>.png and diff_<name>_<frame>.png (the difference, times --gain).
report.tsv in it has a line for every frame compared. The differences that are not above the threshold are counted
and shown as well, nothing is left out of the report.

The frames of the runs are told apart by the number, the time, the set and the setup in frames.tsv: where these differ the runs
did not stay together and the frame is reported as such. Needs Pillow and numpy."""
import argparse
import os
import sys

import numpy as np
from PIL import Image


def read_index(path):
    frames = {}
    last = None
    with open(os.path.join(path, "frames.tsv")) as f:
        for line in f:
            number, ms, name, setup, file = line.rstrip("\n").split("\t")
            if file == "=":
                file = last
            else:
                last = file
            frames[int(number)] = ((int(ms), name, int(setup)), file, line.endswith("\t=\n"))
    return frames


def load(path, file):
    return np.asarray(Image.open(os.path.join(path, file)).convert("RGB"), dtype=np.int16)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("dirs", nargs="+", help="the frame directories, the reference first")
    parser.add_argument("-o", "--output", required=True, help="where the report and the images go")
    parser.add_argument("--threshold", type=int, default=0, help="the difference in a channel (0-255) that counts (default 0)")
    parser.add_argument("--gain", type=int, default=8, help="the factor of the difference image (default 8)")
    parser.add_argument("--every", type=int, default=1, help="compare one frame in this many (default 1)")
    parser.add_argument("--changed", action="store_true", help="only the frames where one of the runs wrote a new picture")
    args = parser.parse_args()
    if len(args.dirs) < 2:
        parser.error("two directories at least")

    names = [os.path.basename(os.path.normpath(d)) for d in args.dirs]
    indexes = [read_index(d) for d in args.dirs]
    os.makedirs(args.output, exist_ok=True)
    report = open(os.path.join(args.output, "report.tsv"), "w")
    report.write("frame\tms\tset\tsetup\tagainst\tpixels\tabove\tmax\tmean\tbbox\n")

    ref = indexes[0]
    for i in range(1, len(indexes)):
        other, name = indexes[i], names[i]
        print("== %s against %s" % (name, names[0]))
        total = identical = below = above = lost = 0
        for number in sorted(set(ref) & set(other)):
            if number % args.every:
                continue
            meta, ref_file, ref_same = ref[number]
            meta2, file, same = other[number]
            if args.changed and ref_same and same:
                continue
            total += 1
            if meta != meta2:
                lost += 1
                print("frame %d: the runs differ, %s %s against %s" % (number, names[0], meta, meta2))
                report.write("%d\t%d\t%s\t%d\t%s\tdesync\t\t\t\t\n" % (number, meta[0], meta[1], meta[2], name))
                continue
            a = load(args.dirs[0], ref_file)
            b = load(args.dirs[i], file)
            if a.shape != b.shape:
                lost += 1
                print("frame %d: sizes %s and %s" % (number, a.shape, b.shape))
                continue
            diff = np.abs(a - b).max(axis=2)
            pixels = int((diff > 0).sum())
            if not pixels:
                identical += 1
                continue
            hit = diff > args.threshold
            count = int(hit.sum())
            box = ""
            if count:
                ys, xs = np.nonzero(hit)
                box = "%d,%d-%d,%d" % (xs.min(), ys.min(), xs.max(), ys.max())
                above += 1
            else:
                below += 1
            mean = float(diff.mean())
            report.write("%d\t%d\t%s\t%d\t%s\t%d\t%d\t%d\t%.4f\t%s\n" % (number, meta[0], meta[1], meta[2], name, pixels, count, diff.max(), mean, box))
            if count:
                print("frame %d  %d ms  %s/%d  %d px differ, %d above %d, max %d, mean %.3f, box %s"
                      % (number, meta[0], meta[1], meta[2], pixels, count, args.threshold, diff.max(), mean, box))
                tag = "%06d" % number
                Image.fromarray(a.astype(np.uint8)).save(os.path.join(args.output, "%s_%s.png" % (names[0], tag)))
                Image.fromarray(b.astype(np.uint8)).save(os.path.join(args.output, "%s_%s.png" % (name, tag)))
                Image.fromarray(np.clip(np.abs(a - b) * args.gain, 0, 255).astype(np.uint8)).save(
                    os.path.join(args.output, "diff_%s_%s.png" % (name, tag)))
        print("%d frames compared: %d identical, %d differ by %d or less in every channel, %d differ by more, %d not comparable, %d only in one run"
              % (total, identical, below, args.threshold, above, lost, len(set(ref) ^ set(other))))
    report.close()


if __name__ == "__main__":
    sys.exit(main())

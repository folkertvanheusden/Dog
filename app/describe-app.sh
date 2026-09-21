#! /bin/sh

echo -n `git describe --always --dirty`_`git branch --show-current`

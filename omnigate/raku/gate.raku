#!/usr/bin/env raku
use v6;
chdir %*ENV<CREATION_OS_ROOT> // '.';
my $p := run 'make', 'merge-gate', :in($*IN), :out($*OUT), :err($*ERR);
exit $p.exitcode;

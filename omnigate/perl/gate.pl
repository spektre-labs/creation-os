#!/usr/bin/env perl
use strict;
use warnings;
chdir $ENV{CREATION_OS_ROOT} // '.';
exec 'make', 'merge-gate' or die "exec make: $!";

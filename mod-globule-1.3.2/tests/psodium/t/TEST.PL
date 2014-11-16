#file:directives/TEST.PL
#-----------------------
# Program to run a series of tests. Unlike other TEST.PL programs it does not
# include all .conf.in files in $t_dir/conf into the main httpd.conf. The
# external script that calls this supplies the .conf file to use via the
# -postamble option.
#
# 	Arno
#
#!perl

use strict;
use warnings FATAL => 'all';

use GlobuleTest::TestConfigDirec();
use base 'Apache::TestRun';
main::->new->run( @ARGV );

sub new_test_config {
    my $self = shift;
    GlobuleTest::TestConfigDirec->new($self->{conf_opts});
}
	

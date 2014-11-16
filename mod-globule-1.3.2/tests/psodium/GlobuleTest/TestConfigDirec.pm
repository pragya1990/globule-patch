package GlobuleTest::TestConfigDirec; # must be

use strict;
use warnings FATAL => 'all';

use Apache::TestConfig ();
#use Apache::TestConfigParse ();
use base 'Apache::TestConfig';

# Arno: Override original method to prevent all *.conf.in in the config dir
# to be Included in the generated httpd.conf. I don't want all as I want
# to test directive processing by selectively including files.
#
sub generate_httpd_conf {
    my $self = shift;
    my $vars = $self->{vars};

    #generated httpd.conf depends on these things to exist
    $self->generate_types_config;
    $self->generate_index_html;

    $self->gendir($vars->{t_logs});

    my @very_last_postamble = ();
    if (my $extra_conf = $self->generate_extra_conf) {
        for my $file (@$extra_conf) {
            my $entry;
            if ($file =~ /\.conf$/) {
                next if $file =~ m|/httpd\.conf$| || !($file =~ m|/extra\.conf$|);
                $entry = qq(Include "$file");
            }
            #elsif ($file =~ /\.pl$/) 
            #{
            #    $entry = qq(<IfModule mod_perl.c>\n    PerlRequire "$file"\n</IfModule>\n);
            #}
            else {
                next;
            }

            # put the .last includes very last
            if ($file =~ /\.last\.(conf)$/) {
                 push @very_last_postamble, $entry;
            }
            else {
                $self->postamble($entry);
            }

        }
    }

    $self->configure_proxy;

    my $conf_file = $vars->{t_conf_file};
    my $conf_file_in = join '.', $conf_file, 'in';

    my $in = $self->httpd_conf_template($conf_file_in);

    my $out = $self->genfile($conf_file);

    $self->preamble_run($out);

    for my $name (qw(user group)) { #win32/cygwin do not support
        if ($vars->{$name}) {
            print $out "\u$name    $vars->{$name}\n";
        }
    }

    #2.0: ServerName $ServerName:$Port
    #1.3: ServerName $ServerName
    #     Port       $Port
    my @name_cfg = $self->servername_config($vars->{servername},
                                            $vars->{port});
    for my $pair (@name_cfg) {
        print $out "@$pair\n";
    }

    $self->replace_vars($in, $out);

    # handle the case when mod_alias is built as a shared object
    # but wasn't included in the system-wide httpd.conf
    my $mod_alias = $self->find_apache_module('mod_alias.so');
    if ($mod_alias && -e $mod_alias) {
        print $out <<EOF;
<IfModule !mod_alias.c>
    LoadModule alias_module "$mod_alias"
</IfModule>
EOF
    }

    print $out "<IfModule mod_alias.c>\n";
    for (keys %Apache::TestConfig::aliases) {
        next unless $vars->{$Apache::TestConfig::aliases{$_}};
        print $out "    Alias /getfiles-$_ $vars->{$Apache::TestConfig::aliases{$_}}\n";
    }
    print $out "</IfModule>\n";

    print $out "\n";

    $self->postamble_run($out);

    print $out join "\n", @very_last_postamble;

    close $in;
    close $out or die "close $conf_file: $!";
}

# Die, Larry, die: "Apache::TestConfigDirec did not return a true value at ..."
1;

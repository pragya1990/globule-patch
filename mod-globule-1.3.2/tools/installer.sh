#!/bin/bash

## CONFIGURABLE SETTINGS #####################################################

#
# The "defaultprefix" may be set to any location which is presented to the
# user as the default prefix location where to install globule and related
# packages.
#
defaultprefix='/usr/local/globule'

#
# This defines the standard port to use
#
if [ "`perl -e 'print $>;'`" = 0 ]; then
  defaultport=80
else
  defaultport=8333
fi

#
# Whether or not to disable the use of MySQL
# Set to 0 to enable MySQL by default, set 1 one to compile, but not
# start, 2 skips entirely.
#
defaultdisablemysql=0

#
# The downloadurl is the location where the installer will fetch
# new packages from.  It may not end with an / which is automatically
# appended.
# In this location a "auto" directory should exist and an auto/src
# directory containing the packages available for automatic download
# The downloadurl
#
downloadurl='ftp://world.cs.vu.nl/pub/globule'

## FIXED SETTINGS ############################################################

#
# Do not change these or anything below unless you know what you are doing.
#
stage=3
verbose=0
optnodownload=0
optnodownloadatreinstall=1
optinstalldemo=1
optkeepbuild=0
optomitbuild=0
optomitstart=0
optextrabuild=
optextraphpconfig=
optextraglobuleconfig=
optextramysqlconfig=
optextragdconfig=
optextrahttpdconfig=
optextrawebalizerconfig=
optomitinstall=0
optinstallerreport=0

## PARSE ARGUMENTS ###########################################################

options=""
while [ $# -gt 0 ]; do
  case "$1" in
  --unshar)
    stage=2
    break
    ;;
  --reinstall)
    stage=1
    ;;
  --continue)
    stage=4
    ;;
  --build)
    stage=5
    ;;
  -v|--verbose)
    verbose=1
    options="$options $1"
    ;;
  -n|--noupdate)
    optnodownload=1
    options="$options $1"
    ;;
  --install-demo|--installdemo)
    optinstalldemo=1
    options="$options $1"
    ;;
  --keep-build)
    optkeepbuild=1
    options="$options $1"
    ;;
  --omit-build)
    optomitbuild=1
    options="$options $1"
    ;;
  --omit-start)
    optomitstart=1
    optextrabuild="$optextrabuild $1"
    options="$options $1"
    ;;
  --omit-install)
    optextrabuild="$optextrabuild $1"
    options="$options $1"
    optomitinstall=1
    ;;
  --extra-php-config=*|--extra-globule-config=*|--extra-mysql-config=*|--extra-gd-config=*|--extra-httpd-config=*|--extra-webalizer-config=*)
    optextrabuild="$optextrabuild $1"
    options="$options $1"
    case "$1" in
    --extra-php-config=*)
      optextraphpconfig="$optextraphpconfig `echo $1 | sed 's/^--extra-[a-z-]*=\(.*\)$/\1/'`"
      ;;
    --extra-globule-config=*)
      optextraglobuleconfig="$optextraglobuleconfig `echo $1 | sed 's/^--extra-[a-z-]*=\(.*\)$/\1/'`"
      ;;
    --extra-mysql-config=*)
      optextramysqlconfig="$optextramysqlconfig `echo $1 | sed 's/^--extra-[a-z-]*=\(.*\)$/\1/'`"
      ;;
    --extra-gd-config=*)
      optextragdconfig="$optextragdconfig `echo $1 | sed 's/^--extra-[a-z-]*=\(.*\)$/\1/'`"
      ;;
    --extra-httpd-config=*)
      optextrahttpdconfig="$optextrahttpdconfig `echo $1 | sed 's/^--extra-[a-z-]*=\(.*\)$/\1/'`"
      ;;
    --extra-webalizer-config=*)
      optextrawebalizerconfig="$optextrawebalizerconfig `echo $1 | sed 's/^--extra-[a-z-]*=\(.*\)$/\1/'`"
      ;;
    esac
    ;;
  --installer-report)
    optinstallerreport=1
    ;;
  --help)
    cat <<END

Usage: $0 [--install-demo] [--keep-build] [--noupdate]
          [--reinstall] [-v|--verbose]
The --install-demo option forces to install demo html pages when the
    installer scripts detect an installed globule configuration.  This
    will overwrite any existing content in the htdocs directory.  For
    the initial installation the demo pages will be included automatically.
With the --keep-build option, the installer script will leave the
    build sources directory, otherwise the build directories will be
    removed afterwards.
In case of upgrading, the installer script will try to fetch updates
    of the modules from the internet.  The --noupdate script will
    prevent doing this and the existing modules will be used.
The --reinstall option will force the installer script to make a clean
    initial installation, overwriting all configuration and html pages,
    even if an existing installation is detected.
The -v or --verbose options increase the verbose output.

END
    exit 0
    ;;
  *)
    printf "$0: unknown argument $1\n"
    exit 1
    ;;
  esac
  shift
done

cd `dirname $0`
script=`pwd`/`basename $0`
cd ..
prefix=`pwd`
# if we wouldn't have RedHat aka Microsoft-dx, we could simply use
# concatenate `hostname` and `domainname`.  RedHat isn't posix compliant
# when setting the FQHN as hostname in the kernel.
# some people even set hostname to the fqhn AND set a domainname, resulting
# in a double domainname.
if [ "`domainname`" = "(none)" ]; then
  defaultfqhn="`hostname`"
else
  defaultfqhn="`hostname | cut -f1 -d .`.`domainname`"
fi
fqhn=$defaultfqhn
port=$defaultport
disablemysql=$defaultdisablemysql
cutdirs=`echo "$downloadurl" | tr -cd / | wc -c`
# URL have two more / (ftp://) and we have "auto" in the end url to recompute
cutdirs=`expr $cutdirs - 2 + 1`

tar=`which gtar 2>/dev/null || which tar`
make=`which gmake 2>/dev/null || which make`
untar() {
  if $tar --version 2>&1 | grep "GNU tar" > /dev/null
  then
    $tar --extract --no-same-owner --gunzip --file $1
  else
    gzip -c -d < $1 | $tar xof -
  fi
}

## STAGE 1 ###################################################################

if [ $stage -eq 1 ]; then

prefix=

cat <<END



This is the globule source installer.

It will download the latest versions of the Globule source and related
software for you, compile and install everything.

First, you have to choose where you want to have globule.  A sensible
default is $defaultprefix.  If you have already some kind of Globule or
other software in this directory which was not installed by this
installer, you should remove that content first.  The destination
directory should be used exclusively by the Globule installation.
This installer will try to preserve any content in the htdocs, htbin and
etc directory when re-running the installer to update your software, but
we provide no guarantee, so backup your data first.
To bail out, press Control+C, otherwise enter the prefix of the directory
where to install.
END

printf "\nWhere do you want to install Globule? [$defaultprefix] "
read prefix
if [ -z "$prefix" ]; then
  prefix="$defaultprefix"
fi
printf "\nWhat is the fully qualified hostname to be used? [$defaultfqhn] " fqhn
read fqhn
if [ -z "$fqhn" ]; then
  fqhn="$defaultfqhn"
fi
printf "\nOn which port do you want to run Apache/Globule? [$defaultport] "
read port
if [ -z "$port" ]; then
  port="$defaultport"
fi
while :; do
  question=`if [ $defaultdisablemysql -eq 0 ];then echo Y/n;else echo y/N;fi`
  printf "\nShould the MySQL database be enabled? [$question] "
  read disablemysql
  if [ -z "$disablemysql" ]; then
    disablemysql=$defaultdisablemysql
    break
  else if [ x"$disablemysql" = xy -o x"$disablemysql" = xY ]; then
    disablemysql=0
    break;
  else if [ x"$disablemysql" = xc -o x"$disablemysql" = xC ]; then
    disablemysql=1
    break
  else if [ x"$disablemysql" = xs -o x"$disablemysql" = xS -o \
            x"$disablemysql" = xn -o x"$disablemysql" = xN ]; then
    disablemysql=2
    break
  else
    printf "incorrect answer\n"
  fi ; fi ; fi ; fi
done

if [ \! -d "$prefix" ]; then
  mkdir "$prefix"
fi
if [ \! -d "$prefix/src" ]; then
  mkdir "$prefix/src"
fi

printf "\nBuilding installation (this might take 5-15 minutes on recent hardware)...\n\n"

args="-e 's/^stage=1$/stage=3/' "
for subst in port fqhn disablemysql ; do
  key="`echo \\$default$subst`"
  val="`echo \\$$subst`"
  #if [ x"`eval echo $key`" != x"`eval echo $val`" ]; then
    args="-e 's/^ *default$subst=.*$/default$subst=`eval echo $val`/' $args"
  #fi
done
eval "sed < $script > $prefix/src/`basename $0`~ $args"
mv "$prefix/src/`basename $0`"~ "$prefix/src/`basename $0`"
chmod a+x "$prefix/src/`basename $0`"
$prefix/src/`basename $0` --unshar
if [ -n "$disablemysql" -a "$disablemysql" -eq 2 ]; then
  rm -f $prefix/src/mysql-*
fi
sed -n < $prefix/src/`basename $0` > $prefix/src/`basename $0`~ -e '/which '\
'uudecode/q' -e 'p'
mv "$prefix/src/`basename $0`"~ "$prefix/src/`basename $0`"
chmod a+x "$prefix/src/`basename $0`"
if [ $optnodownloadatreinstall -eq 0 ]; then
  exec $prefix/src/`basename $0` $options
else
  exec $prefix/src/`basename $0` --noupdate $options
fi

fi

## STAGE 3 ###################################################################

if [ $stage -eq 3 ]; then

if [ $optnodownload -ne 1 ] ; then
  # FIXME: wget is real standard by know, and we depend on it internaly too.
  if [ \! -x "`which wget`" ] ; then
    printf "You seem not to have wget installed.  This is required for"
    printf " automatic\nupdates.\n"
    exit 1
  fi

  printf "\nDownloading and updating the installer\n"
  wget -nv -O src/`basename $0`~ "$downloadurl/auto/`basename $0`"
  if [ -s src/`basename $0`~ ] ; then
    args=""
    for subst in port fqhn disablemysql ; do
      key="`echo \\$default$subst`"
      val="`echo \\$$subst`"
      #if [ x"`eval echo $key`" != x"`eval echo $val`" ]; then
        args="-e 's/^ *default$subst=.*$/default$subst=`eval echo $val`/' $args"
      #fi
    done
    rm -f src/`basename $0`
    if [ -n "$args" ]; then
      eval "sed < src/`basename $0`~ > src/`basename $0` $args"
    else
      cat < src/`basename $0`~ > src/`basename $0`
    fi
    chmod a+x "src/`basename $0`"
  fi
fi

printf "\nWARNING: do not interrupt the installer operation, your installation"
printf " will\nbe corrupted.  Original directories can be recovered from the"
printf " src directory.\n";

exec $prefix/src/`basename $0` --continue $options

fi

## STAGE 4 ###################################################################

if [ $stage -eq 4 ]; then

if [ $optnodownload -ne 1 ] ; then
  printf "\nDownloading and updating the packages\n"
  args="-nv -m -N -nH --cut-dirs=$cutdirs"
  if [ $disablemysql -ge 2 ]; then
    args="$args -R mysql-\*"
  fi
  eval "wget $args \"$downloadurl/auto/src/\""
fi

rm -f $prefix/src/httpd.conf~
if [ \! -e $prefix/etc/httpd.conf ]; then
  optinstalldemo=1
  cp $prefix/src/httpd.conf $prefix/src/httpd.conf~
fi
rm -f $prefix/src/webalizer.conf~
if [ \! -e $prefix/etc/webalizer.conf ]; then
  cp $prefix/src/webalizer.conf $prefix/src/webalizer.conf~
fi

args=
if [ $optinstalldemo -ne 0 ]; then args="$args --install-demo" ; fi
if [ $optomitbuild   -ne 0 ]; then args="$args --omit-build"   ; fi
if [ $optkeepbuild   -ne 0 ]; then args="$args --keep-build"   ; fi

eval "$prefix/src/`basename $0` 3>&2 > $prefix/src/installer.log 2>&1 --build --installer-report $args $optextrabuild"
# --omit-start

# Now, we are skipping over stage 5 into stage 6.
stage=6
fi

## STAGE 5 ###################################################################

if [ $stage -eq 5 ]; then

if [ $optomitinstall -ne 1 -o $optomitstart -ne 1 ]; then
  if [ -x $prefix/bin/globulectl ]; then
    $prefix/bin/globulectl stop
  elif [ -x $prefix/bin/apachectl ]; then
    $prefix/bin/apachectl stop
  fi
  if [ -x $prefix/bin/mysqladmin ]; then
    $prefix/bin/mysqladmin -u master shutdown
  fi
fi

# Find command to GNU specific (-maxdepth), use a for instead to remove
# old stuff
#find . -maxdepth 1 \! \( -name . -o -name .. -o -name src -o -name etc -o \
#                         -name htdocs -o -name htbin \) -exec rm -rf \{\} \;
if [ $optomitinstall -eq 0 ]; then
  for f in * ; do
    case "$f" in
      src|etc|htdocs|htbin|var|logs)
        ;;
      *.berry)
        rm -rf "$f"
        ;;
      *.*)
        ;;
      *)
        rm -rf "$f"
        ;;
    esac
  done
  mv etc    src 2>/dev/null
  mv htbin  src 2>/dev/null
  mv htdocs src 2>/dev/null
  mv var    src 2>/dev/null
  mv logs   src 2>/dev/null
fi

cd src

globule_version=`ls -1 mod-globule-*.tar.gz | sed -e 's/mod-globule-\([0-9]*\)\.\([0-9]*\)\.\([0-9]*\)\.tar\.gz/\1 \2 \3/p' -e 'd' | sort -k 1n -k 2n -k 3n | tail -1 | awk '{printf("%d.%d.%d",$1,$2,$3);}'`
php_version=`ls -1 php-*.tar.gz | sed -e 's/php-\([0-9]*\)\.\([0-9]*\)\.\([0-9]*\)\.tar\.gz/\1 \2 \3/p' -e 'd' | sort -k 1n -k 2n -k 3n | tail -1 | awk '{printf("%d.%d.%d",$1,$2,$3);}'`
smarty_version=`ls -1 Smarty-*.tar.gz | sed -e 's/Smarty-\([0-9]*\)\.\([0-9]*\)\.\([0-9]*\)\.tar\.gz/\1 \2 \3/p' -e 'd' | sort -k 1n -k 2n -k 3n | tail -1 | awk '{printf("%d.%d.%d",$1,$2,$3);}'`
httpd_version=`ls -1 httpd-*.tar.gz | sed -e 's/httpd-\([0-9]*\)\.\([0-9]*\)\.\([0-9]*\)\.tar\.gz/\1 \2 \3/p' -e 'd' | sort -k 1n -k 2n -k 3n | tail -1 | awk '{printf("%d.%d.%d",$1,$2,$3);}'`
mysql_version=`ls -1 mysql-*.tar.gz | sed -e 's/mysql-\([0-9]*\)\.\([0-9]*\)\.\([0-9]*\)\.tar\.gz/\1 \2 \3/p' -e 'd' | sort -k 1n -k 2n -k 3n | tail -1 | awk '{printf("%d.%d.%d",$1,$2,$3);}'`
gd_version=`ls -1 gd-*.tar.gz | sed -e 's/gd-\([0-9]*\)\.\([0-9]*\)\.\([0-9]*\)\.tar\.gz/\1 \2 \3/p' -e 'd' | sort -k 1n -k 2n -k 3n | tail -1 | awk '{printf("%d.%d.%d",$1,$2,$3);}'`
webalizer_version=`ls -1 webalizer-*-src.tgz | sed -e 's/webalizer-\([0-9]*\)\.\([0-9]*\)-\([0-9]*\)-src\.tgz/\1 \2 \3/p' -e 'd' | sort -k 1n -k 2n -k 3n | tail -1 | awk '{printf("%d.%s-%d",$1,$2,$3);}'`

echo ""
echo "Installing in      : $prefix"
echo "Globule version    : $globule_version"
echo "Apache version     : $httpd_version"
echo "PHP version        : $php_version"
echo "Smarty version     : $smarty_version"
echo "MySQL version      : $mysql_version"
echo "GD library version : $gd_version"
echo "Webalizer version  : $webalizer_version"
echo ""
if [ $optinstallerreport -ne 0 ]; then
  printf >&3 "\n"
  printf >&3 "Installing in      : $prefix\n"
  printf >&3 "Globule version    : $globule_version\n"
  printf >&3 "Apache version     : $httpd_version\n"
  printf >&3 "PHP version        : $php_version\n"
  printf >&3 "Smarty version     : $smarty_version\n"
  printf >&3 "MySQL version      : $mysql_version\n"
  printf >&3 "GD library version : $gd_version\n"
  printf >&3 "Webalizer version  : $webalizer_version\n"
  printf >&3 "\n"
fi

if [ -f mod-globule-$globule_version.tar.gz ]; then
  if [ \! -d mod-globule-$globule_version -o \( $optomitbuild -eq 0 -a mod-globule-$globule_version.tar.gz -nt mod-globule-$globule_version/config.status \) ]; then
    if [ $optinstallerreport -ne 0 ]; then
      printf >&3 "unpacking Globule..."
    fi
    untar mod-globule-$globule_version.tar.gz
    if [ $optinstallerreport -ne 0 ]; then
      printf >&3 "  done.\n"
    fi
  fi
fi

case "$httpd_version" in
2.0.*)
  httpd_match="^2.0"
  ;;
2.2.*)
  httpd_match="^2.2"
  ;;
*)
  httpd_match=""
  if [ $optinstallerreport -ne 0 ]; then
    printf >&3 "Patching unrecognized Apache version may fail\n"
  fi
  ;;
esac
cd mod-globule-$globule_version/apache
patch_version=`ls -1 udp-requests-httpd-*.patch | sed -e 's/udp-requests-httpd-\([0-9]*\)\.\([0-9]*\)\.\([0-9]*\)\.patch/\1 \2 \3/p' -e 'd' | grep "$httpd_match" | sort -k 1n -k 2n -k 3n | tail -1 | awk '{printf("%d.%d.%d",$1,$2,$3);}'`
cd ../..

if [ -f mysql-$mysql_version.tar.gz ]; then
  if [ \! -d mysql-$mysql_version -o \( $optomitbuild -eq 0 -a mysql-$mysql_version.tar.gz -nt mysql-$mysql_version/config.status \) ]; then
    if [ $optinstallerreport -ne 0 ]; then
      printf >&3 "unpacking MySQL..."
    fi
    untar mysql-$mysql_version.tar.gz
    if [ $optinstallerreport -ne 0 ]; then
      printf >&3 "  done.\n"
    fi
  fi
fi
if [ -d mysql-$mysql_version ]; then
cd mysql-$mysql_version
if [ $optomitbuild -eq 0 -a ../mysql-$mysql_version.tar.gz -nt config.status ]; then
  if [ $optinstallerreport -ne 0 ]; then
    printf >&3 "compiling MySQL..."
  fi
  eval "./configure --prefix=$prefix --without-debug --with-embedded-server --with-unix-socket-path=$prefix/var/mysql.sock --disable-static --without-debug --without-mysqlmanager $optextramysqlconfig"
  $make
  rtcode=$?
  if [ $optinstallerreport -ne 0 ]; then
    if [ $rtcode -eq 0 ]; then
      printf >&3 "  done.\n"
    else
      printf >&3 "  failed; your installation will have limited functionality.\n"
    fi
  fi
fi
if [ $optomitinstall -eq 0 ]; then
  if [ $optinstallerreport -ne 0 ]; then
    printf >&3 "installing MySQL..."
  fi
  $make install
  if [ \! -f $prefix/etc/my.conf ]; then
    if [ \! -d $prefix/etc ]; then
      mkdir $prefix/etc
    fi
    cat <<-END > $prefix/etc/my.conf
	END
  if [ $optinstallerreport -ne 0 ]; then
    printf >&3 "  done.\n"
  fi
  fi
fi
cd ..
fi

if [ -f httpd-$httpd_version.tar.gz ]; then
  if [ \! -d httpd-$httpd_version -o \( $optomitbuild -eq 0 -a httpd-$httpd_version.tar.gz -nt httpd-$httpd_version/config.status \) ]; then
    if [ $optinstallerreport -ne 0 ]; then
      printf >&3 "unpacking Apache..."
    fi
    untar httpd-$httpd_version.tar.gz
    if [ $optinstallerreport -ne 0 ]; then
      printf >&3 "  done.\n"
    fi
  fi
fi
cd httpd-$httpd_version
patch -p0 -b -V numbered -N < ../mod-globule-$globule_version/apache/udp-requests-httpd-$patch_version.patch
if [ $optomitbuild -eq 0 -a ../httpd-$httpd_version.tar.gz -nt config.status ]; then
  if [ $optinstallerreport -ne 0 ]; then
    printf >&3 "compiling Apache..."
  fi
  eval "./configure --prefix=$prefix --sysconfdir=$prefix/etc --enable-auth-anon=shared --enable-authn-dbm=shared --enable-authn-anon=shared --enable-auth-dbm=shared --enable-cache=shared --enable-file-cache=shared --enable-disk-cache=shared --enable-mem-cache=shared --enable-example=shared --enable-deflate=shared --disable-ssl --enable-proxy=shared --enable-proxy-ftp=shared --enable-proxy-http=shared --enable-proxy-connect=shared --enable-expires=shared --enable-headers --enable-mime-magic --enable-http --enable-dav=shared --enable-status=shared --enable-asis=shared --enable-suexec=shared --enable-info=shared --enable-cgi=shared --enable-include=shared --enable-vhost-alias=shared --enable-rewrite=shared --enable-logio=shared $optextrahttpdconfig"
  $make
  rtcode=$?
  if [ $optinstallerreport -ne 0 ]; then
    if [ $rtcode -eq 0 ]; then
      printf >&3 "  done.\n"
    else
      printf >&3 "  FAILED; your installation will not work at all.\n"
    fi
  fi
fi
if [ $optomitinstall -eq 0 ]; then
  if [ $optinstallerreport -ne 0 ]; then
    printf >&3 "installing Apache..."
  fi
  $make install
  if [ $optinstallerreport -ne 0 ]; then
    printf >&3 "  done.\n"
  fi
fi
cd ..

if [ -f gd-$gd_version.tar.gz ]; then
  if [ \! -d gd-$gd_version -o \( $optomitbuild -eq 0 -a gd-$gd_version.tar.gz -nt gd-$gd_version/config.status \) ]; then
    if [ $optinstallerreport -ne 0 ]; then
      printf >&3 "unpacking GD..."
    fi
    untar gd-$gd_version.tar.gz
    if [ $optinstallerreport -ne 0 ]; then
      printf >&3 "  done.\n"
    fi
  fi
fi
if [ -d gd-$gd_version ]; then
cd gd-$gd_version
if [ $optomitbuild -eq 0 -a ../gd-$gd_version.tar.gz -nt config.status ]; then
  if [ $optinstallerreport -ne 0 ]; then
    printf >&3 "compiling GD..."
  fi
  eval "./configure --prefix=$prefix $optextragdconfig"
  $make
  rtcode=$?
  if [ $optinstallerreport -ne 0 ]; then
    if [ $rtcode -eq 0 ]; then
      printf >&3 "  done.\n"
    else
      printf >&3 "  failed; your installation will have limited functionality.\n"
    fi
  fi
fi
if [ $optomitinstall -eq 0 ]; then
  if [ $optinstallerreport -ne 0 ]; then
    printf >&3 "installing GD..."
  fi
  $make install
  if [ $optinstallerreport -ne 0 ]; then
    printf >&3 "  done.\n"
  fi
fi
cd ..
fi

if [ -f php-$php_version.tar.gz ]; then
  if [ \! -d php-$php_version -o \( $optomitbuild -eq 0 -a php-$php_version.tar.gz -nt php-$php_version/config.status \) ]; then
    if [ $optinstallerreport -ne 0 ]; then
      printf >&3 "unpacking PHP..."
    fi
    untar php-$php_version.tar.gz
    if [ $optinstallerreport -ne 0 ]; then
      printf >&3 "  done.\n"
    fi
  fi
fi
if [ -d php-$php_version ]; then
cd php-$php_version
if [ $optomitbuild -eq 0 -a ../php-$php_version.tar.gz -nt config.status ]; then
  if [ $optinstallerreport -ne 0 ]; then
    printf >&3 "compiling PHP..."
  fi
  if which >/dev/null 2>&1 curl-config ; then optextraphpconfig="--with-curl=yes $optextraphpconfig" ; fi
  if [ -x $prefix/bin/mysql_config ]; then optextraphpconfig="--with-mysql=$prefix --with-mysqli=$prefix/bin/mysql_config --with-mysql-sock=$prefix/var/mysql.sock $optextraphpconfig" ; fi
  eval "./configure  --prefix=$prefix --with-apxs2filter=$prefix/bin/apxs --disable-cgi --enable-safe-mode --with-gdbm --with-ini --with-flatfile --with-gd=$prefix --enable-ftp --disable-static $optextraphpconfig"
  $make
  rtcode=$?
  if [ $optinstallerreport -ne 0 ]; then
    if [ $rtcode -eq 0 ]; then
      printf >&3 "  done.\n"
    else
      printf >&3 "  failed; your installation will have limited functionality.\n"
    fi
  fi
fi
if [ $optomitinstall -eq 0 ]; then
  if [ $optinstallerreport -ne 0 ]; then
    printf >&3 "installing PHP..."
  fi
  $make install
  if [ $optinstallerreport -ne 0 ]; then
    printf >&3 "  done.\n"
  fi
fi
cd ..
fi

if [ -f Smarty-$smarty_version.tar.gz ]; then
  if [ \! -d Smarty-$smarty_version -o \( $optomitbuild -eq 0 -a Smarty-$smarty_version.tar.gz -nt Smarty-$smarty_version/config.status \) ]; then
    if [ $optinstallerreport -ne 0 ]; then
      printf >&3 "unpacking Smarty..."
    fi
    untar Smarty-$smarty_version.tar.gz
    if [ $optinstallerreport -ne 0 ]; then
      printf >&3 "  done.\n"
    fi
  fi
fi
if [ -d Smarty-$smarty_version ]; then
cd Smarty-$smarty_version
if [ $optomitinstall -eq 0 ]; then
  if [ $optinstallerreport -ne 0 ]; then
    printf >&3 "installing Smarty..."
  fi
  cp -r libs/* $prefix/lib/php/.
  if [ $optinstallerreport -ne 0 ]; then
    printf >&3 "  done.\n"
  fi
fi
cd ..
fi

cd mod-globule-$globule_version
if [ $optomitbuild -eq 0 -a ../mod-globule-$globule_version.tar.gz -nt config.status ]; then
  if [ $optinstallerreport -ne 0 ]; then
    printf >&3 "compiling Globule..."
  fi
  args=
  if [ $optinstalldemo -ne 0 ]; then args="$args --enable-globuleadm" ; fi
  if [ $optomitinstall -ne 0 ]; then args="$args --disable-dependency-tracking" ; fi
  eval "./configure --with-apache=$prefix --disable-static --disable-psodium --without-openssl --enable-dns-redirection $args $optextraglobuleconfig"
  $make clean
  $make
  rtcode=$?
  if [ $optinstallerreport -ne 0 ]; then
    if [ $rtcode -eq 0 ]; then
      printf >&3 "  done.\n"
    else
      printf >&3 "  FAILED; your installation will not work.\n"
    fi
  fi
fi
if [ $optomitinstall -eq 0 ]; then
  if [ $optinstallerreport -ne 0 ]; then
    printf >&3 "installing Globule..."
  fi
  $make install
  if [ $optinstallerreport -ne 0 ]; then
    printf >&3 "  done.\n"
  fi
fi
cd ..

# May not compile if libpng is missing
if [ -f webalizer-$webalizer_version-src.tgz ]; then
  if [ \! -d webalizer-$webalizer_version -o \( $optomitbuild -eq 0 -a webalizer-$webalizer_version-src.tgz -nt webalizer-$webalizer_version/config.status \) ]; then
    if [ $optinstallerreport -ne 0 ]; then
      printf >&3 "unpacking Webalizer..."
    fi
    untar webalizer-$webalizer_version-src.tgz 
    if [ $optinstallerreport -ne 0 ]; then
      printf >&3 "  done.\n"
    fi
  fi
fi
if [ -d webalizer-$webalizer_version ]; then
cd webalizer-$webalizer_version
if [ $optomitbuild -eq 0 -a ../webalizer-$webalizer_version-src.tgz -nt config.status ]; then
  if [ $optinstallerreport -ne 0 ]; then
    printf >&3 "compiling Webalizer..."
  fi
  eval "./configure --prefix=$prefix --with-etcdir=$prefix/etc --with-gdlib=$prefix/lib --with-gd=$prefix/include $optextrawebalizerconfig"
  $make
  rtcode=$?
  if [ $optinstallerreport -ne 0 ]; then
    if [ $rtcode -eq 0 ]; then
      printf >&3 "  done.\n"
    else
      printf >&3 "  failed; most things will still work for your installation.\n"
    fi
  fi
fi
if [ $optomitinstall -eq 0 ]; then
  if [ $optinstallerreport -ne 0 ]; then
    printf >&3 "installing Webalizer..."
  fi
  $make install
  if [ $optinstallerreport -ne 0 ]; then
    printf >&3 "  done.\n"
  fi
fi
cd ..
fi

if [ $optinstallerreport -ne 0 ]; then
  printf >&3 "Preparing the installed system...\n"
fi

if [ $optomitinstall -eq 0 ]; then
if [ -d mysql-$mysql_version ]; then
if [ \! -e ../var/mysql/user.frm ]; then
pushd $prefix > /dev/null
patch -p0 -b -c -N <<END
*** bin/mysql_install_db.orig	2005-05-02 16:12:28.000000000 +0200
--- bin/mysql_install_db	2005-05-03 11:23:10.000000000 +0200
***************
*** 242,248 ****
      echo "to the right place for your system"
      echo
    fi
!   if test "\$windows" -eq 0
    then
    echo "PLEASE REMEMBER TO SET A PASSWORD FOR THE MySQL root USER !"
    echo "To do so, start the server, then issue the following commands:"
--- 242,248 ----
      echo "to the right place for your system"
      echo
    fi
!   if test "\$windows" -eq 0 -a 0 -eq 1
    then
    echo "PLEASE REMEMBER TO SET A PASSWORD FOR THE MySQL root USER !"
    echo "To do so, start the server, then issue the following commands:"

END
./bin/mysql_install_db --defaults-file=$prefix/etc/my.conf --rpm
if [ "`perl -e 'print $>;'`" = 0 ]; then
  ./bin/mysqld_safe --defaults-file=$prefix/etc/my.conf --skip-networking --user=root 2>/dev/null >/dev/null &
else
  ./bin/mysqld_safe --defaults-file=$prefix/etc/my.conf --skip-networking             2>/dev/null >/dev/null &
fi
sleep 13 ;# give MySQL time to start
./bin/mysqladmin -u root password ""
./bin/mysqladmin -u root -h localhost password ""
./bin/mysql -u root mysql <<-END
	grant all on *.* to 'master'@'localhost' identified by '';
	grant shutdown on *.* to  'master'@'localhost' identified by '';
END
./bin/mysqladmin -u master shutdown
popd > /dev/null
fi
fi
if [ -d ../var ]; then chmod go+x ../var ; fi
fi

if [ $optomitinstall -eq 0 ]; then

pushd $prefix > /dev/null
if [ \! -d tmp ]; then
  mkdir tmp
fi
if [ -d src/var ]; then
  if [ -d var ]; then
    mv var var.berry
  fi
  mv src/var var
fi
if [ -d src/logs ]; then
  if [ -d logs ]; then
    mv logs logs.berry
  fi
  mv src/logs logs
fi
if [ -d src/etc ]; then
  mv etc etc.berry
  mv src/etc etc
  if [ \! -f etc/my.conf ]; then cp etc.berry/my.conf etc ; fi
  cp etc.berry/httpd-globule.conf etc
  cp etc.berry/magic etc.berry/mime.types etc.berry/pear.conf etc
fi
if [ -d src/htdocs ]; then
  mv htdocs htdocs.berry
  mv src/htdocs htdocs
  if [ \( \! -d htdocs/globuleadm \) -o -d htdocs/globuleadm/templates ] ; then
    cp -r htdocs.berry/globuleadm/. htdocs/globuleadm
  fi
fi
if [ -d src/htbin ]; then
  if [ -d htbin ]; then
    mv htbin htbin.berry
  fi
  mv src/htbin htbin
else
  if [ \! -d htbin ]; then
    mkdir htbin
    touch htbin.berry
  fi
fi
if [ -d cgi-bin ]; then
  mv cgi-bin cgi-bin.berry
fi
if [ -d error ]; then
  mv error error.berry
fi
if [ -d icons ]; then
  mv icons icons.berry
fi
if [ -d sql-bench ]; then
  mv sql-bench sql-bench.berry
fi
if [ -d mysql-test ]; then
  mv mysql-test mysql-test.berry
fi
rm -rf *.berry
popd > /dev/null

cd mod-globule-$globule_version
if [ $optinstalldemo -ne 0 ]; then
  #find $prefix/htdocs/. \! -name . \( -name globuleadm -prune -o \
  #                             -exec rm -rf \{\} \; \)
  $make install-demo
fi
rm -f $prefix/etc/run-webalizer.sh  ; # remove remnants from 1.3.1
rm -f $prefix/etc/installcrontab.sh ; # remove remnants from 1.3.1
rm -f $prefix/etc/installconf.sh    ; # remove remnants from 1.3.1
rm -f $prefix/src/build.sh          ; # remove remnants from 1.3.1
cd ..

fi

if [ $optkeepbuild -eq 0 ]; then
  rm -rf mysql-$mysql_version
  rm -rf httpd-$httpd_version
  rm -rf php-$php_version
  rm -rf mod-globule-$globule_version
  rm -rf webalizer-$webalizer_version
  rm -rf gd-$gd_version
fi

if [ $optomitstart -eq 0 -a $optinstallerreport -eq 0 ]; then
  if [ -x $prefix/bin/globulectl ]; then
    $prefix/bin/globulectl start
  elif [ -x $prefix/bin/apachectl ]; then
    if [ "`perl -e 'print $>;'`" = 0 ]; then
      ./bin/mysqld_safe --defaults-file=$prefix/etc/my.conf --skip-networking --user=root 2>/dev/null >/dev/null &
    else
      ./bin/mysqld_safe --defaults-file=$prefix/etc/my.conf --skip-networking             2>/dev/null >/dev/null &
    fi
    $prefix/bin/apachectl start
  fi
fi

exit 0

fi

## STAGE 6 ###################################################################

if [ $stage -eq 6 ]; then

printf "\nInitializing and starting services\n"

if [ -n "$disablemysql" -a "$disablemysql" -ne 0 ] ; then
  if [ -x $prefix/bin/mysqld_safe ]; then
    chmod a-x $prefix/bin/mysqld_safe 2>/dev/null
  fi
fi

if [ -e $prefix/src/httpd.conf~ ]; then
  $prefix/bin/globulectl installconf -real \
     source=$prefix/src/httpd.conf~ target=$prefix/etc/httpd.conf \
     confdir=etc fqhn=$fqhn port=$port
  rm $prefix/src/httpd.conf~
fi
if [ -f $prefix/htdocs/globuleadm/.htaccess ]; then
  sed < $prefix/htdocs/globuleadm/.htaccess "s/localhost *\$/localhost $fqhn/" \
      > $prefix/htdocs/globuleadm/.htaccess~
  mv $prefix/htdocs/globuleadm/.htaccess~ $prefix/htdocs/globuleadm/.htaccess
fi
if [ -f $prefix/src/webalizer.conf~ ]; then
  $prefix/bin/globulectl installconf source=$prefix/src/webalizer.conf~ \
     target=$prefix/etc/webalizer.conf confdir=etc fqhn=$fqhn port=$port
  rm $prefix/src/webalizer.conf~
fi

if [ $optomitstart -eq 0 ]; then
  if [ -x $prefix/bin/globulectl ]; then
    $prefix/bin/globulectl start
  elif [ -x $prefix/bin/apachectl ]; then
    if [ -x $prefix/bin/mysqld_safe ]; then
      if [ "`perl -e 'print $>;'`" = 0 ]; then
        ./bin/mysqld_safe --skip-networking --user=root >/dev/null 2>&1 &
      else
        ./bin/mysqld_safe --skip-networking             >/dev/null 2>&1 &
      fi
    fi
    $prefix/bin/apachectl start
  fi
fi

cat <<END
==============================================================================

END
if [ "uname -s" = "Linux" ]; then
if [ "`cat /proc/sys/kernel/shmmax 2>/dev/null || echo 0`" -lt 16777216 ]; then
cat <<END
The installer script has detected that your system has a shared memory
segment which is smaller than a normal system which Globule assumes.
Either instruct Globule to use less shared memory or increase the limit.
Please refer to the Globule documentation and /proc/sys/kernel/shmmax

END
fi
if [ "`awk -f /proc/sys/kernel/sem '{print$4;}' 2>/dev/null || echo 0`" -lt 64 ]; then
cat <<END
The installer script has detected that your system has an average number
of semaphores.  Globule uses quite a lot of semaphores, and it may be
necessary to increase the number of semaphores made available by the kernel.
For a quick start perform the operation:
  echo "256 32000 32 1024" > /proc/sys/kernel/sem
As root, but please refer also to the Globule documentation.

END
fi
fi

cat <<END

Hopefully the build should have finished correcly, example data should
have been installed, and the services should have been started.  However
you might want to inspect $prefix/src/installer.log for errors.  If
the build had failed, please inform us and provide us with the installer.log
and any error messages you might see above.
To test the server and view example content, go to the address
  `sed < $prefix/etc/httpd.conf -e 's/^ServerName \(.*\)/http:\/\/\1\//p' -e d`
At any future moment you may upgrade the software by running the
installer at  $prefix/src/installer.sh
(this is not necessarily the original you just executed).

Have fun!
The Globule Team.

END

fi

## STAGE 2 ###################################################################

if [ $stage -ne 2 ]; then
  exit 0
fi

if [ -z "`which uudecode 2>/dev/null`" ]; then
cat <<END > src/uudecode
#!`which perl`
while(<>) {
  if(/^begin [0-7][0-7][0-7] ([^\\n ]+)\$/) {
    open (OUTPUT, ">\$1") || die "Cannot create \$1\\n";
    binmode OUTPUT;
    while(<>) {
      last if /^end\$/;
      \$block = unpack("u", \$_);
      print OUTPUT \$block;
    }
    close OUTPUT;
  }
}
exit 0;
END
chmod u+x src/uudecode
PATH=$PATH:$prefix/src
fi
#if [ -z "`which uudecode 2>/dev/null`" ]; then
#cat <<END > src/uudecode.c
#
#END
#CC="`which gcc 2>/dev/null || which cc`"
#if [ -z "$CC" ]; then
#  echo "$0: cannot find uudecode, gcc, or cc to obtain a uudecode"
#  exit 1
#fi
#CC -o src/uudecode src/uudecode.c
#PATH=$PATH:$prefix/src
#if [ -z "`which uudecode 2>/dev/null`" ]; then
#  echo "$0: tried to make my own uudecode, did not work"
#fi
#fi


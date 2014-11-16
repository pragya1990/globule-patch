#!/bin/sh

#
# This script generates the shell-script-installer "installer.sh".
# The "installer.sh" script can be downloaded by semi-end-users
# which want a fully automated method of compiling Globule and
# its prerequisites and utility software packages, but want to
# have a home-build/compiled Globule and not a binary only installation
#
# The "installer.sh" is basically a script and a shar-archive in one.
# The script has basically a number of simple functions.
# - unpack itself (the shar-archive which is appended at the end
# - possibly retrieve new updates from the web site (currently this is
#   omitted if the shar-archive is present, the shar archive is disabled
#   from the installer.sh script afterwards when placed in the target
#   directory).
# - create the destination directory and place the sources in it.  Normally
#   the destination is /usr/local/globule and the sources are placed in
#   /usr/local/globule/src.
# - build the sources and install the software
# - install a demo httpd.conf and install demo web pages.
#
# The installer.sh script is both used as a complete all-in-one installer
# script which contains all sources as well as a script which retrieves
# upgrades of the sources from the web and re-installs the software.
# To this end, the script detects whether it contains an enabled shar
# archive at the end (it patches itself when it installs itself to disable
# the shar archive) and whether the software has previously installed.
#
# The model is that the initial "installer.sh" script downloaded from the
# web (normally ftp://ftp.globule.org/pub/globule/installer.sh) contains
# the full sources as a shar archive.  When everything is installed, the
# core install.sh script with disabled shar archive is placed in
# /usr/local/globule/src.  Then called again, or when the software is
# reinstalled, the installer.sh script will try to update this installer.sh
# from the web (normally ftp://ftp.globule.org/pub/globule/auto/installer.sh)
# Note that everything in the auto/ directory is suppost to be fetched by
# the install.sh script to update existing packages.  Also updated
# individual software distributions are fetched from this location, however
# from the auto/src/ subdirectory to facilate building and extracting the
# shar archive.
#
# This script either pushes a true release, a prerelease or a testing
# release.  Without command line arguments, a testing release is
# produced.  A prerelease is pushed when the parameter "prerelease" is
# given as first command line argument, and a full release is pushed
# when the command line argument is "release".
# For a full release, the install.sh script in BOTH the primary location
# ftp://ftp.globule.org/pub/globule/install.sh (for the full shar archive)
# as the updater script (same, but without shar archive) in
# ftp://ftp.globule.org/pub/globule/auto/install.sh
# Note that the files are updated based on the distribution file, not
# copied from the location of this mkinstall.sh script.
#
# Placing updated packages of mod-globule*.tar.gz and the other software
# packages in ftp://ftp.globule.org/pub/globule/auto/src is the
# responsibility of the maintainer and falls outside the scope of this
# script.
#
# The paths for the prerelease and testing release are
# /home/ftp/pub/globule/prerelease/ and /home/ftp/pub/globule/testing/
# respectively.
# Note that the full shar archive is also made under a different name,
# namely "prerelease.sh" or "testing.sh" instead of "installer.sh".
#

destination=/home/ftp/pub/globule

cd `dirname $0`/..
rm -f mod-globule-*.tar.gz
make dist

case "$1" in
release)
  releaseas=release
  ;;
prerelease)
  releaseas=prerelease
  ;;
*)
  releaseas=testing
  ;;
esac

case $releaseas in
prerelease)
  cp mod-globule-*.tar.gz /home/ftp/pub/globule/prerelease/src/
  cd $destination/prerelease
  target=prerelease.sh
  ;;
testing)
  cp mod-globule-*.tar.gz /home/ftp/pub/globule/testing/src/
  cd $destination/testing
  target=testing.sh
  ;;
release)
  cp mod-globule-*.tar.gz /home/ftp/pub/globule/auto/src/
  cd $destination/auto
  target=installer.sh
  ;;
*)
  exit 1
esac

if [ "`domainname`" = "(none)" ]; then
  fqhn="`hostname`"
else
  fqhn="`hostname | cut -f1 -d .`.`domainname`"
fi

tar xOzf src/mod-globule* \
  `tar tzf src/mod-globule* | egrep /tools/installer.sh$` > installer.sh 
chmod 755 installer.sh
case $releaseas in
release)
  sed < installer.sh > installer.sh~ \
    -e "s/^\(downloadurl=.[a-z]*\):\/\/[^\/]*\/\(.*\)$/\1:\/\/$fqhn\/\2/"
  mv installer.sh~ $target
  chmod 755 $target
  ;;
prerelease)
  sed < installer.sh > installer.sh~ \
    -e 's/\$downloadurl\/auto/\$downloadurl\/prerelease/g' \
    -e "s/^\(downloadurl=.[a-z]*\):\/\/[^\/]*\/\(.*\)$/\1:\/\/$fqhn\/\2/"
  mv installer.sh~ $target
  chmod 755 $target
  ;;
testing)
  sed < installer.sh > installer.sh~ \
    -e 's/\$downloadurl\/auto/\$downloadurl\/testing/g' \
    -e "s/^\(downloadurl=.[a-z]*\):\/\/[^\/]*\/\(.*\)$/\1:\/\/$fqhn\/\2/"
  mv installer.sh~ $target
  chmod 755 $target
  ;;
esac
tar xOzf src/mod-globule* \
  `tar tzf src/mod-globule* | egrep /sample/httpd.conf$` > src/httpd.conf
tar xOzf src/mod-globule* \
  `tar tzf src/mod-globule* | egrep /sample/webalizer.conf$` > src/webalizer.conf
sed < $target > ../$target \
  -e 's/^stage=3$/stage=1/'
shar >> ../$target -s berry@cs.vu.nl -n globule -p -x \
  -T src/httpd.conf \
  -B `find src -type f -maxdepth 1 \! -name httpd.conf`
chmod 755 ../$target

exit 0

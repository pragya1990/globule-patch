{literal}#!/bin/sh

. `dirname $0`/globule.cgi

echo "Content-Type: text/html"
echo ""

shmperc=`expr \( \( $shmbytes + $shmitems \* 8 \) \* 100 \* 10 + $shmsize \* 5 \) / $shmsize`
shmperc=`expr $shmperc / 10`.`expr $shmperc % 10`

if [ -z "$gbs" ]; then
  gbs="site does not use the Globule Broker System"
fi

cat <<EOF
<html><head>
  <title>Globule Controls</title>
  <link rel="SHORTCUT ICON" href="http://www.globule.org/favicon.ico" />
  <link href="Globule.css" rel="stylesheet" title="preferred" />
</head><body>
  <center>
  [ <a class="topnav" href="index.html">index</a>
  | <a class="topnav" href="general.cgi">general</a>
  | <a class="topnav" href="sections.cgi">sections</a>
  ]
  </center>
  <table class="pagemain"><tr><td>
    <h1>Globule Controls</h1>
    <hr class="titlehr"/>
    <h2>General Information</h2>
    <hr class="sectionhr"/>
    <p>
      <form action="general.php" METHOD="POST" ENCTYPE="application/x-www-form-urlencoded">
      <table cellspacing="3" cellpadding="0" border="0"><tr>
        <td>PHP support</td>
        <td align="center">:</td>
        <td><!-- <?php echo "--", ">", "enabled", "<", "!--"; // -->disabled; only basic information will be available<!-- ?>
	  -->
          </td>
      </tr><tr>
        <td>Globule version</td>
        <td align="center">:</td>
        <td>$data_version</td>
      </tr><tr>
        <td>Broker serial</td>
        <td align="center">:</td>
        <td>$gbs</td>
      </tr><tr>
        <td>Shared memory usage</td>
        <td align="center">:</td>
        <td>${shmperc}%; approximately $shmbytes of $shmsize in $shmitems items</td>
      </tr></table>
      </form>
    </p>
    <center>
      <img src="chart.php?black=t+v+shm2" alt="shared memory usage graph">
    </center>
  <br/><br/><br/><br/><br/><br/>
  </td></tr></table>
    </p>
</body></html>
EOF
{/literal}

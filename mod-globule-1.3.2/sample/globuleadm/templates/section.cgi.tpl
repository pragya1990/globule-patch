{literal}#!/bin/sh

. `dirname $0`/globule.cgi

echo "Content-Type: text/html"
echo ""

section_reevaluateinterval=`expr $section_reevaluateinterval \* $section_refreshinterval`
if [ -z "$section_npeers" ]; then section_npeers=0 ; fi

cat <<EOT
<html><head>
  <title>Globule Controls</title>
  <link rel="SHORTCUT ICON" href="http://www.globule.org/favicon.ico" />
  <link href="Globule.css" rel="stylesheet" title="preferred" />
  <meta http-equiv="refresh" content="3">
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
    <h2>Details of section</h2>
    <hr class="sectionhr"/>
    <p>
      <table cellspace="3" cellpadding="3" border="0">
      <tr>
        <th colspan="3" align="left"><a href="$section_uri">$section_uri</a></th>
      </tr><tr>
        <td rowspan="8">&nbsp;</td>
        <td>Type:</td>
        <td>$section_type&nbsp;</td>
      </tr><tr>
        <td>ServerName:</td>
        <td>$section_servername&nbsp;</td>
      </tr><tr>
        <td>Path:</td>
        <td>$section_path&nbsp;</td>
      </tr><tr>
        <td>Redirection policy:</td>
        <td>$section_redirectpolicy&nbsp;</td>
      </tr><tr>
        <td>Default replication policy:</td>
        <td>$section_replicatepolicy&nbsp;</td>
      </tr><tr>
        <td>Refresh interval:</td>
        <td>$section_refreshinterval s&nbsp;</td>
      </tr><tr>
        <td>Reevaluate interval:</td>
        <td>$section_reevaluateinterval&nbsp;</td>
      </tr><tr>
        <td colspan="2">
            [ <a href="section.cgi?url=$section_uri"]">refresh</a>
            | <a href="section.cgi?url=$section_uri"]&action=flush">flush</a>
            ]<br/>
          </td>
      </tr><tr>
        <td colspan="3">&nbsp;</td>
      </tr>
      </table>
    </p>
    <h3>Peers</h3>
    <table class="infotable">
    <tr class="infotr">
      <th class="infoth">name</th>
      <th class="infoth">type</th>
      <th class="infoth">available</th>
    </tr>
EOT

i=0
while [ $i -lt $section_npeers ] ; do
  class=`expr \( $i / 3 \) % 2 + 1`
  eval section_peer=\$section_peer${i}
  eval section_peer_uri=\$section_peer${i}_uri
  eval section_peer_type=\$section_peer${i}_type
  eval section_peer_available=\$section_peer${i}_available
  case "$section_peer_available" in
  true) section_peer_available="<font color=green>yes</font>" ;;
  *)    section_peer_available="<font color=red>no</font>" ;;
  esac

  cat <<EOT
    <tr class="infotr$class">
      <td class="infotd"><a href="$section_peer_uri">$section_peer&nbsp;</td>
      <td class="infotd">$section_peer_type&nbsp;</td>
      <td class="infotd">$section_peer_available&nbsp;</td>
    </tr>
EOT

  i=`expr $i + 1`
done

cat <<EOT
    </table></br>
EOT

cat <<EOF
    </table>
  <br/><br/><br/><br/><br/><br/>
  </td></tr></table>
</body></html>
EOF
{/literal}

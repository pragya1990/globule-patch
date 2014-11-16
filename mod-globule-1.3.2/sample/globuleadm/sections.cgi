#!/bin/sh

. `dirname $0`/globule.cgi

echo "Content-Type: text/html"
echo ""

cat <<EOT
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
    <h2>Sections</h2>
    <hr class="sectionhr"/>
    <p>
      <table cellspace="3" cellpadding="3" border="0">
EOT

i=0
while [ $i -lt $nsections ]; do
  eval section_uri=\$section${i}_uri
  eval section_type=\$section${i}_type
  eval section_path=\$section${i}_path
  eval section_servername=\$section${i}_servername
  case "$section_type" in
  database) section_typetxt="database" ;;
  *)        section_typetxt="section"  ;;
  esac

  cat <<EOT
      <tr>
        <th colspan="3" align="left"><a href="$section_uri">$section_uri</a></th>
      </tr><tr>
        <td rowspan="4">&nbsp;</td>
        <td>Type:</td>
        <td>$section_type&nbsp;</td>
      </tr><tr>
        <td>ServerName:</td>
        <td>$section_servername&nbsp;</td>
      </tr><tr>
        <td>Path:</td>
        <td>$section_path&nbsp;</td>
      </tr><tr>
        <td colspan="2">
            [ <a href="$section_typetxt.cgi?url=$section_uri">details</a>
            ]<br/>
          </td>
      </tr><tr>
        <td colspan="3">&nbsp;</td>
      </tr>
EOT

  i=`expr $i + 1`
done

cat <<EOF
      </table>
    </p>
  <br/><br/><br/><br/><br/><br/>
  </td></tr></table>
</body></html>
EOF

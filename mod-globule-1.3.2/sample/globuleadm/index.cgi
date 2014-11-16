#!/bin/sh

echo "Content-Type: text/html"
echo ""

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
    <h2>Introduction</h2>
    <hr class="sectionhr"/>
    <p>
    These web pages allow you to view, monitor and alter certain statistics
    and settings of the Globule system.
    </p>
  <br/><br/><br/><br/><br/><br/>
  </td></tr></table>
</body></html>
EOF

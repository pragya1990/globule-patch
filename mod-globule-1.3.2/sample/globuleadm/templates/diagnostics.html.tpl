<html><head>
  <title>Globule Controls</title>
  <link rel="SHORTCUT ICON" href="http://www.globule.org/favicon.ico" />
  <link href="Globule.css" rel="stylesheet" title="preferred" />
</head><body>
  <center>
  [ <a class="topnav" href="index.html">index</a>
  | <a class="topnav" href="general.html">general</a>
  | <a class="topnav" href="diagnostics.html"><b>diagnostics</b></a>
  ]
  </center>
  <table class="pagemain"><tr><td>
    <h1>Globule Controls</h1>
    <hr class="titlehr"/>
    <h2>Diagnostics</h2>
    <hr class="sectionhr"/>
    <p>
{literal}
    <p>

    As part of its operation, Globule writes messages into the error-log when
    it encounters certain circumstances that may be of interest to its
    administrator.
    Besides being written into the log files, the latest messages can also be
    reviewed through this web-interface.

    The messages are classified depending on their seriousness,
    such as plain informational, warnings, errors, and critical messages
    and so on.  You can select through which extend you want to have
    these messages logged:
    </p>
    <form action="diagnostics.php" METHOD="POST" ENCTYPE="application/x-www-form-urlencoded">
      <select name="profile">
        <option selected>Choose one</option>
        <option>normal</option>
        <option>extended</option>
        <option>verbose</option>
      </select>
      <input type="SUBMIT" name="SUBMIT" value="select">
    </form>
{/literal}
    </p>
  <br/><br/><br/><br/><br/><br/>
  </td></tr></table>
</body></html>

<?PHP require("globule.php"); ?>
<html><head>
  <title>Globule Administration</title>
  <link rel="SHORTCUT ICON" href="http://www.globule.org/favicon.ico" />
  <link href="Globule.css" rel="stylesheet" title="preferred" />

  <script language="javascript"><!--
var pinned=false;
function showBalloon(ev) {
  if(window.ballooncontent && !pinned) {
    var balloon = document.getElementById("balloon");
    if(!ev)
      ev = window.event;
    var yPos = ev['clientY'] + document.body.scrollTop + 16;
    var xPos = ev['clientX'] + document.body.scrollLeft - 108;
    balloon.style.top = yPos + "px";
    balloon.style.left = xPos + "px";
    balloon.innerHTML = ' <table bgcolor="#FFFFFF" cellspacing="0" cellpadding="3" style="border:1px solid #000066;"><tr><td bgcolor="#C0C0C0"><!-- Information -->&nbsp;</td><td bgcolor="#C0C0C0" align="right">[ <a href="#" onClick="javascript:void(self.pinned=false);">close</a> ]</td></tr><tr><td colspan="2" id="balloontext"><font face="arial" size="1">' + window.ballooncontent + '</font></td></tr></table>';
    balloon.style.visibility = "visible";
  }
}
function hideBalloon() {
  if(!pinned) {
    var balloon = document.getElementById("balloon");
    balloon.style.visibility = "hidden";
    window.ballooncontent = "";
  }	 
}
function balloon(content) {
  window.ballooncontent=content;
}
function doLoad() {
  window.document.onmouseover = showBalloon;
  window.document.onmouseout = hideBalloon;
}
  // --></script>

</head><body onLoad="javascript:doLoad()">
<div id="balloon" style="position:absolute;visibility:hidden;width:450px;"></div>

<?PHP if(file_exists("site-".$_SERVER["SERVER_NAME"].".png")){?>
<img align="right" src="site-<?=$_SERVER["SERVER_NAME"]?>.png" alt="<?=$_SERVER["SERVER_NAME"]?>">
<?PHP } else { ?>
<div style="float:right"><?=$_SERVER["SERVER_NAME"]?></div>
<?PHP } ?>

  <center>
    [ <a class="topnav" href="./">index</a>
    | <a class="topnav" href="general.php">general</a>
    | <a class="topnav" href="diagnostics.php"><b>diagnostics</b></a>
    | <a class="topnav" href="sections.php">sections</a>
<!--
    | <a class="topnav" href="namebinding.php">namebinding</a>
-->
    ]
  </center><br clear="all"/>
  <table class="pagemain"><tr><td>
    <h1>Globule Administration</h1>
    <hr class="titlehr"/>
    <h2>Diagnostics</h2>
    <hr class="sectionhr"/>
    <p>

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
    <h3>Error reporting</h3>
    <hr class="subsectionhr"/>
    <p>
      Latest error messages:
      <?PHP globuleGetInfo("msgs=x2"); ?>
      <blockquote><pre><?PHP
        passthru('cd ../.. ; if [ -r logs/error.log -a logs/error.log -nt etc/httpd.conf ]; then tail -30 logs/error.log ; else echo "Error file not accessible" ; fi');
      ?></pre></blockquote>
    </p>

    </p>
  <br/><br/><br/><br/><br/><br/>
  </td></tr></table>
    <p><script language="php">
      if(strstr($_SERVER['REQUEST_URI'],"?phpinfo")) {
        echo "<hr/>";
        phpinfo();
      }
    </script></p>
</body></html>

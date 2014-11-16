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
    [ <a class="topnav" href="./"><b>index</b></a>
    | <a class="topnav" href="general.php">general</a>
    | <a class="topnav" href="diagnostics.php">diagnostics</a>
    | <a class="topnav" href="sections.php">sections</a>
<!--
    | <a class="topnav" href="namebinding.php">namebinding</a>
-->
    ]
  </center><br clear="all"/>
  <table class="pagemain"><tr><td>
    <h1>Globule Administration</h1>
    <hr class="titlehr"/>
    <h2>Index</h2>
    <hr class="sectionhr"/>
    <p>
  Welcome to the administration interface of server
  <b><code><?=$_SERVER["SERVER_NAME"]?></code></b> owned by
  <a href="mailto:<?=$_SERVER["SERVER_ADMIN"]?>"><?=$_SERVER["SERVER_ADMIN"]
  ?></a>.
  </p><p>
  This web page allows you to view, monitor and alter certain
  statistics and settings of this server.
  </p><p>
  On the top of this web-page is a menu to the different items available.
  Browse through them to see what's available.  When on a particular web-page,
  you may hoover with your mouse over items to get an explanation.
  </p><p>
  For a general explanation about <a href="http://www.globule.org/">Globule</a>
  and its configuration, please read the
  <a href="http://www.globule.org/docs/">user manual</a>.
  </p><p>
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

<?PHP require("globule.php"); ?>
<html><head>
  <title>Globule Administration</title>
  <link rel="SHORTCUT ICON" href="http://www.globule.org/favicon.ico" />
  <link href="Globule.css" rel="stylesheet" title="preferred" />
  <meta http-equiv="refresh" content="60">

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
    | <a class="topnav" href="diagnostics.php">diagnostics</a>
    | <a class="topnav" href="sections.php">sections</a>
<!--
    | <a class="topnav" href="namebinding.php"><b>namebinding</b></a>
-->
    ]
    <br clear="all"/>
&lt; <i><a class="topnav" href="section.php?url=<?= $_REQUEST["url"] ?>">section details</a></i>

  </center><br clear="all"/>
  <table class="pagemain"><tr><td>
    <h1>Globule Administration</h1>
    <hr class="titlehr"/>
    <h2>Section peers</h2>
    <hr class="sectionhr"/>
    <p>

      The web-site section <a href="<?= $_REQUEST["section_uri"] ?>"><?= $_REQUEST["section_uri"] ?></a> has <?= $_REQUEST["section_npeers"] ?> peers, which help out your web-server in different ways.  Of each partner below it's role in helping you is listed, as well as some other information.
      <!-- ?php
        echo("<PRE>");
        globuleInfo($values, "m=info+t+v+availability");
	foreach(sort($values) as $k => $v) {
	  echo $k, " = ", $v, "\n";
	}
        echo("</PRE>");
      ? -->
    </p><p>
      <table cellspace="3" cellpadding="3" border="0">
      <?PHP for($i=0; $i<$_REQUEST["section_npeers"]; $i++) { ?>
      <tr>
        <th colspan="3" align="left"><a href="<?= $_REQUEST["section_peer".$i."_uri"] ?>"><?= $_REQUEST["section_peer".$i."_uri"] ?></a></th>
      </tr><tr>
        <td rowspan="4">&nbsp;</td>
        <td>This peer has the role of:</td>
        <td><?= $_REQUEST["section_peer".$i."_type"] ?>&nbsp;</td>
      </tr><tr>
        <!--
        <td>The mutual password to recognize each other is:</td>
        <td>?= $_REQUEST["section_peer".$i."_secret"] >&nbsp;</td> -->
      </tr><tr>
        <td>This peer is currently available:</td>
        <td><?php if($_REQUEST["section_peer".$i."_available"]=="true") { echo "yes"; } else { echo "no"; if($_REQUEST["section_peer".$i."_enabled"]<0) echo ", manually permanently disabled"; } ?>&nbsp;</td>
      </tr><tr>
        <td>The last time the availability state changed was:</td>
        <td><?= globuleFormatTime($_REQUEST["section_peer".$i."_serial"]) ?>&nbsp;</td>
      </tr><tr>
        <td colspan="3">&nbsp;</td>
      </tr>
      <?PHP } ?>
      </table>

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

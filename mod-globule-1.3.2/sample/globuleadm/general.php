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
    | <a class="topnav" href="general.php"><b>general</b></a>
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
    <h2>General Information</h2>
    <hr class="sectionhr"/>
    <p>

    <hr class="sectionhr"/>
    <p>
    This page shows generic information about the software and machine the
    Globule-enabled web-server.
    </p><p>
    This Globule-enabled web-server is running the following software and
    configuration settings:
    </p><p>
      <form action="general.php" METHOD="POST" ENCTYPE="application/x-www-form-urlencoded">

      <table cellspacing="3" cellpadding="0" border="0"><tr>
        <td>PHP support</td>
        <td align="right">&nbsp;<a href="#" onClick="javascript:void(self.pinned=true);" onmouseover="balloon('PHP allows a web-site builder to use dynamic content.  Pages where the content is customized per request, such as displaying the results of a search query, forums discussions, user preferences, and so on.')"><img src="i.png" border="0" alt="[info]"></a></td>
        <td align="center">:</td>
        <td><!-- <?php echo "--", ">", "enabled", "<", "!--"; // -->disabled; most information will not be available<!-- ?>
	  -->
          </td>
      </tr><tr>
        <td>Globule version</td>
        <td align="right">&nbsp;<a href="#" onClick="javascript:void(self.pinned=true);" onmouseover="balloon('It is always recommended that you use the latest version.  Up-to-date versions are available <a href=http://www.globule.org/download/>here</a>.  Also, please comunicate this version number to us if you need help about Globule.')"><img src="i.png" border="0" alt="[info]"></a></td>
        <td align="center">:</td>
        <td><?= $_REQUEST['version'] ?></td>
      </tr><tr>
        <td>Apache version</td>
        <td align="right">&nbsp;<a href="#" onClick="javascript:void(self.pinned=true);" onmouseover="balloon('Globule is running on Apache software.  You automatically keep up-to-date with the latest Apache software by updating your Globule version when using the installer.')"><img src="i.png" border="0" alt="[info]"></a></td>
        <td align="center">:</td>
        <td><?= $_SERVER["SERVER_SOFTWARE"] ?></td>
      </tr><tr>
        <td>Broker serial</td>
        <td align="right">&nbsp;<a href="#" onClick="javascript:void(self.pinned=true);" onmouseover="balloon('If your server is registered with the GBS, then the GBS will generate config files automatically.  To make sure that your server uses the latest generated version of its config file, check that this serial num,ber is identical to the one mentioned by the GBS.  If it is not, then you should restart your server.')"><img src="i.png" border="0" alt="[info]"></a></td>
        <td align="center">:</td>
        <td><?= array_key_exists('gbs',$_REQUEST) && strcmp($_REQUEST['gbs'],"") ? $_REQUEST['gbs'] : "server does not use the Globule Broker System" ?></td>
      <!--
      </tr><tr>
        <td>Test string</td>
        <td align="right">&nbsp;<a href="#" onClick="javascript:void(self.pinned=true);" onmouseover="balloon('For testing purposes only')"><img src="i.png" border="0" alt="[info]"></a></td>
        <td align="center">:</td>
        <td><script language="php">
            echo ( "<input type=\"text\" name=\"test\" value=\"" . $_REQUEST['test'] . "\"/>" );
	    </script>&nbsp;
          </td>
      -->
      </tr></table>
      </form>

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

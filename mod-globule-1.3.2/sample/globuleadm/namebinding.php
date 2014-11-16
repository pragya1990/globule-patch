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
    | <a class="topnav" href="diagnostics.php">diagnostics</a>
    | <a class="topnav" href="sections.php">sections</a>
<!--
    | <a class="topnav" href="namebinding.php"><b>namebinding</b></a>
-->
    ]
  </center><br clear="all"/>
  <table class="pagemain"><tr><td>
    <h1>Globule Administration</h1>
    <hr class="titlehr"/>
    <h2>NameBinding</h2>
    <hr class="sectionhr"/>
    <p>

      <form action="namebinding.php" METHOD="POST" ENCTYPE="application/x-www-form-urlencoded">
      <table class="infotable"><tr class="infotr">
        <th class="infoth">name</th>
        <th class="infoth">record type</th>
        <th class="infoth">refers to</th>
      </tr><tr class="infotr">
        <?php for($i=0; $i<$_REQUEST["nbindings"]+1; $i++) { ?>
        <?php ; } ?>
        <td class="infotd"><input type="text<?= $i ?>" width="32" value="<?= $_REQUEST["name".$i] ?>"></td>
        <td class="infotd"><select name="type">
	    <option>choose..</option>
            <option<?= !strcasecmp($_REQUEST["type".$i],"A") ? " selected" : "" ?>>A</option>
	    <option<?= !strcasecmp($_REQUEST["type".$i],"CNAME") ? " selected" : "" ?>>CNAME</option>
	    </select></td>
        <td><input type="param<?= $i ?>" width="48" value="<?= $_REQUEST["param".$i] ?>"></td>
        </tr><tr>
      </tr></table>
      <input type="hidden" name="nbindings" value="<?= $_REQUEST["nbindings"]+1 ?>" />
      <input type="SUBMIT" name="SUBMIT" value="set" />
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

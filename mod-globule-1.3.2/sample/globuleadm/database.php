<?PHP require("globule.php"); ?>
<html><head>
  <title>Globule Administration</title>
  <link rel="SHORTCUT ICON" href="http://www.globule.org/favicon.ico" />
  <link href="Globule.css" rel="stylesheet" title="preferred" />
  <meta http-equiv="refresh" content="3">

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
    | <a class="topnav" href="sections.php"><b>sections</b></a>
<!--
    | <a class="topnav" href="namebinding.php">namebinding</a>
-->
    ]
  </center><br clear="all"/>
  <table class="pagemain"><tr><td>
    <h1>Globule Administration</h1>
    <hr class="titlehr"/>
    <h2>Details of section</h2>
    <hr class="sectionhr"/>
    <p>

    <p> 
      <table cellspace="3" cellpadding="3" border="0">
      <tr>
        This page shows more details on the Globule-enabled web-site
	database section<br>
        <th colspan="3" align="left"><a href="<?= $_REQUEST["section_uri"] ?>"><?= $_REQUEST["section_uri"] ?></a></th>
      </tr><tr>
        <td rowspan="8">&nbsp;</td>
        <td>The role your server plays for this section is:</td>
        <td><?= $_REQUEST["section_type"] ?>&nbsp;</td>
      </tr><tr>
        <td>The main ServerName for this site is:</td>
        <td><?= $_REQUEST["section_servername"] ?>&nbsp;</td>
      </tr><tr>
        <td>Files are located at the path:</td>
        <td><?= $_REQUEST["section_path"] ?>&nbsp;</td>
      </tr><tr>
        <td colspan="3">&nbsp;</td>
      </tr>
      </table>
    </p>
    <h3>Queries</h3>
    Known queries for this database where:
    <table class="infotable">
    <tr class="infotr">
      <th class="infoth">name</th>
      <th class="infoth">statement</th>
      <th class="infoth">relations</th>
      <th class="infoth">instances</th>
    </tr>
    <?PHP for($i=0; $i<$_REQUEST["section_ndocs"]; $i++) { ?>
    <tr class="infotr<?= floor($i/3)%2+1 ?>">
      <td class="infotd"><?= $_REQUEST["section_doc".$i."_name"] ?>&nbsp;</td>
      <td class="infotd"><?= $_REQUEST["section_doc".$i."_statement"] ?>&nbsp;</td>
      <td class="infotd">
          <?PHP for($j=0; $j<$_REQUEST["section_ndocs"]; $j++)
            echo $_REQUEST["section_doc".$i."_relation".$j];
          ?>
        </td>
      <td class="intofd"><?= $_REQUEST["section_doc".$i."_ndocs"] ?>&nbsp;</td>
    </tr>
    <?PHP } ?>
    </table>
    <h3>Instances</h3>
    
    <table class="infotable">
    <tr class="infotr">
      <th class="infoth">name</th>
      <th class="infoth">statement</th>
      <th class="infoth">relations</th>
      <th class="infoth">instances</th>
    </tr>
    <?PHP for($i=0; $i<$_REQUEST["section_ndocs"]; $i++) { ?>
    <tr class="infotr<?= floor($i/3)%2+1 ?>">
      <td class="infotd"><?= $_REQUEST["section_doc".$i."_name"] ?>&nbsp;</td>
      <td class="infotd"><?= $_REQUEST["section_doc".$i."_statement"] ?>&nbsp;</td>
      <td class="infotd">
          <?PHP for($j=0; $j<$_REQUEST["section_ndocs"]; $j++)
            echo $_REQUEST["section_doc".$i."_relation".$j];
          ?>
        </td>
      <td class="intofd"><?= $_REQUEST["section_doc".$i."_ndocs"] ?>&nbsp;</td>
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

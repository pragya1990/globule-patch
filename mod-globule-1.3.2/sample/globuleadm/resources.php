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
    This web-server is running on the machine host named
    <?=$_SERVER["SERVER_NAME"]?> under the administrative control of
    <?=$_SERVER["SERVER_ADMIN"]?>.  From this machine, the Globule software
    uses certain resources, such as memory and diskspace.
    </p><p>
      <table cellspacing="3" cellpadding="0" border="0"><tr>
      </tr><tr>
        <td>Shared memory usage</td>
        <td align="right">&nbsp;<a href="#" onClick="javascript:void(self.pinned=true);" onmouseover="balloon('Of the total amount of memory, a small piece is assigned to hold shared administrative data.  Below is the amount of this piece used, which should never get filled upto maximum capacity otherwise the web-site will seize functioning.  Note that this shared memory is not the same as your real memory in your computer, just the piece assigned to Globule.')"><img src="i.png" border="0" alt="[info]"></a></td>
        <td align="center">:</td>
        <td><?=round(($_REQUEST['shmused']+$_REQUEST['shmitems']*8)*100/$_REQUEST['shmsize'],1) ?>%; approximately <?= globuleFormatFileSize($_REQUEST['shmused']+$_REQUEST['shmitems']*8) ?> of <?= globuleFormatFileSize($_REQUEST['shmsize']) ?> in <?= $_REQUEST['shmitems'] ?> items<br/>
	    <?=$_REQUEST['shmsize']-$_REQUEST['shmused']-$_REQUEST['shmavail']?> bytes used in overhead (<?=round(($_REQUEST['shmsize']-$_REQUEST['shmused']-$_REQUEST['shmavail'])*100/$_REQUEST['shmsize'],1)?></td>
      </tr>
<?php
  $nsections=$_REQUEST["nsections"];
  for($i=0; $i<$nsections; $i++) {
?>
      <tr>
        <th colspan="3" align="left"><a href="<?= $_REQUEST["section".$i."_uri"] ?>"><?= $_REQUEST["section".$i."_uri"] ?></a></th>
        <td>&nbsp;</td>
        <td>available document slots: <?=$_REQUEST["section".$i."_resource_numdocs_available"]?> / <?=$_REQUEST["section".$i."_resource_numdocs_total"]?></td>
      </tr>
<?php
  }
?>
      </table>
    </p>
    <!--
    <center>
      <img src="chart.php?black=t+v+shm2" alt="shared memory usage">
    </center>
    -->
    <center>
      <img src="chart-map.php?query=dump=meminfo&black=t+v+dump" alt="shared memory usage">
    </center>


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

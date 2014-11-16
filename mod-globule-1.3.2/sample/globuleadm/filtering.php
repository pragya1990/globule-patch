<?PHP require("globule.php"); ?>
<html><head>
  <title>Globule Administration</title>
  <link rel="SHORTCUT ICON" href="http://www.globule.org/favicon.ico" />
  <link href="Globule.css" rel="stylesheet" title="preferred" />
  <meta http-equiv="refresh" content="10">

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
    | <a class="topnav" href="namebinding.php">namebinding</a>
-->
    ]
  </center><br clear="all"/>
  <table class="pagemain"><tr><td>
    <h1>Globule Administration</h1>
    <hr class="titlehr"/>
    <h2>Filtering</h2>
    <hr class="sectionhr"/>
    <p>

    <table border="0"><tr><td valign="top">
    <form action="filtering.php" METHOD="POST" ENCTYPE="application/x-www-form-urlencoded">
      <table class="infotable">
      <tr class="infotr">
        <th class="infoth">name</th>
        <th class="infoth">description</th>
        <th class="infoth">type</th>
        <th class="infoth">arguments</th>
      </tr>
      <?PHP for($i=$_REQUEST["nfilters"]-1; $i>0; $i--) { ?>
      <tr class="infotr<?= floor($i/4)%2+1 ?>">
        <td class="infotd"><?= $_REQUEST["name".$i] ?>&nbsp;</td>
        <td class="infotd"><?= $_REQUEST["desc".$i] ?>&nbsp;</td>
        <td class="infotd"><font size="-1"><select name="type<?= $i ?>">
            <option>null</option>
            <option<?= ($_REQUEST["type".$i]=="log"?" selected":"") ?>><font size=-1>log</font></option>
            <option<?= ($_REQUEST["type".$i]=="trace"?" selected":"") ?>>trace</option>
            <option<?= ($_REQUEST["type".$i]=="counter"?" selected":"") ?>>counter</option>
            <option<?= ($_REQUEST["type".$i]=="duplicate"?" selected":"") ?>>duplicate</option>
            <option<?= ($_REQUEST["type".$i]=="rate"?" selected":"") ?>>rate</option>
            <option<?= ($_REQUEST["type".$i]=="cyclicstorage"?" selected":"") ?>>cyclicstorage</option>
            <?= $_REQUEST["type".$i] ?>
          </select>&nbsp;</font></td>
        <td class="infotd"><?= $_REQUEST["args".$i] ?>&nbsp;</td>
      </tr>
      <?PHP } ?>
      </table></br>
    <input type="hidden" name="nfilters" value="<?PHP echo ( $nfilters ); ?>" />
    <input type="SUBMIT" name="SUBMIT" value="set" />
    </form>
    </td><td valign="top">
      Description of the types of filters available:<p/>
      <table><tr>
        <td>log</td>
	<td>Output anything send through this channel to the error log of
	    Apache.
	  </td>
      </tr><tr>
        <td>trace</td>
	<td>Append information about the event delivered to this filter to
	    the current event, ignoring otherwise.
	  </td>
      </tr><tr>
        <td>counter</td>
	<td>Count the events send to this filter, ignoring event type.
	  </td>
      </tr><tr>
        <td>duplicate</td>
	<td>Duplicate all events received by this filter and forward to
	    the filters specified.
	    Apache.
	  </td>
      </tr><tr>
        <td>rate</td>
	<td>Counts the number of events over an interval of time.
	  </td>
      </tr><tr>
        <td>cyclicstorage</td>
	<td>Remembers events delivered to it, with a maximum to limit
	    the memory usage, replacing old events with new.
	  </td>
      </tr><tr>
    </td></tr></table>

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
<?PHP
	$titles = array (
	  "legacy verbose debugging info",
	  "legacy debugging info level 0",
	  "legacy debugging info level 1",
	  "legacy debugging info level 2",
	  "legacy debugging info level 3",
	  "legacy debugging info level 4",
	  "legacy debugging info level 5",
	  "policy reporting",
	  "policy <tt>none</tt> token",
	  "policy <tt>mirror</tt> token",
	  "policy <tt>proxy</tt> token",
	  "policy <tt>ttl</tt> token",
	  "policy <tt>alex</tt> token",
	  "policy <tt>invalidate</tt> token",
	  "replication method reporting",
	  "replication <tt>fetched</tt> token",
	  "replication <tt>cached</tt> token",
	  "replication <tt>native</tt> token",
	  "replication <tt>error</tt> token",
	  "internal error messages",
	  "plain error messages",
	  "warning messages",
	  "notice messages",
	  "informational messages",
	  "served-by reporting"
	);
?>
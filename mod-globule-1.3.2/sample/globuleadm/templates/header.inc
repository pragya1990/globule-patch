{if $globuled}
<?PHP require("globule.php"); ?>
{/if}
<html><head>
  <title>Globule Administration</title>
  <link rel="SHORTCUT ICON" href="http://www.globule.org/favicon.ico" />
  <link href="Globule.css" rel="stylesheet" title="preferred" />
{if $refreshing}
  <meta http-equiv="refresh" content="{$refreshing}">
{/if}
{literal}
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
{/literal}
</head><body onLoad="javascript:doLoad()">
<div id="balloon" style="position:absolute;visibility:hidden;width:450px;"></div>
{literal}
<?PHP if(file_exists("site-".$_SERVER["SERVER_NAME"].".png")){?>
<img align="right" src="site-<?=$_SERVER["SERVER_NAME"]?>.png" alt="<?=$_SERVER["SERVER_NAME"]?>">
<?PHP } else { ?>
<div style="float:right"><?=$_SERVER["SERVER_NAME"]?></div>
<?PHP } ?>
{/literal}
  <center>
    [ <a class="topnav" href="./">{if $menu eq "index"}<b>index</b>{else}index{/if}</a>
    | <a class="topnav" href="general.php">{if $menu eq "general"}<b>general</b>{else}general{/if}</a>
    | <a class="topnav" href="diagnostics.php">{if $menu eq "diagnostics"}<b>diagnostics</b>{else}diagnostics{/if}</a>
    | <a class="topnav" href="sections.php">{if $menu eq "sections"}<b>sections</b>{else}sections{/if}</a>
<!--
    | <a class="topnav" href="namebinding.php">{if $menu eq "namebinding"}<b>namebinding</b>{else}namebinding{/if}</a>
-->
    ]
{if $submenu ne ""}
    <br clear="all"/>{$submenu}
{/if}
  </center><br clear="all"/>
  <table class="pagemain"><tr><td>
    <h1>{#title#}</h1>
    <hr class="titlehr"/>
    <h2>{$title}</h2>
    <hr class="sectionhr"/>
    <p>


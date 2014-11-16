{config_load file="settings.conf"}{*
*}{capture assign="submenu"}{literal}
&lt; <i><a class="topnav" href="section.php?url=<?= $_REQUEST["url"] ?>">section details</a></i>
{/literal}{/capture}{*
*}{include file="header.inc" menu="namebinding" title="Section peers" globuled=1 refreshing=60}
{literal}
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
{/literal}
{include file="footer.inc"}

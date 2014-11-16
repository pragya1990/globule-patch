{config_load file="settings.conf"}{*
*}{include file="header.inc" menu="sections" title="Sections"}
{literal}
  <p>
  In this page you will find the list of sites which are hosted at your server.  For each site, your server may act as the <a href="http://www.globule.org/htbin/search.cgi?definition=origin">origin</a>, a <a href="http://www.globule.org/htbin/search.cgi?definition=replica">replica</a> or a <a href="http://www.globule.org/htbin/search.cgi?definition=redirector">redirector</a>.
  </p><p>
  To get more information about seach section follow the links.
  </p>
      <table cellspace="3" cellpadding="3" border="0">
<?php
  $nsections=$_REQUEST["nsections"];
  for($i=0; $i<$nsections; $i++) {
?>
      <tr>
        <th colspan="3" align="left"><a href="<?= $_REQUEST["section".$i."_uri"] ?>"><?= $_REQUEST["section".$i."_uri"] ?></a></th>
      </tr><tr>
        <td rowspan="4">&nbsp;</td>
        <td>Role:</td>
        <td><?= $_REQUEST["section".$i."_type"] ?>&nbsp;</td>
      </tr><tr>
        <td>ServerName:</td>
        <td><?= $_REQUEST["section".$i."_servername"] ?>&nbsp;</td>
      </tr><tr>
        <td>Path:</td>
        <td><?= $_REQUEST["section".$i."_path"] ?>&nbsp;</td>
      </tr><tr>
        <td colspan="2">
            [ <a href="<?= ($_REQUEST["section".$i."_type"] == "database" ? "database" : "section" ) ?>.php?url=<?= $_REQUEST["section".$i."_uri"] ?>">details</a>
            ]<br/>
          </td>
      </tr><tr>
        <td colspan="3">&nbsp;</td>
      </tr>
<?php
  }
?>
      </table>
{/literal}
{include file="footer.inc"}

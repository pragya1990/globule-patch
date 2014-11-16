{config_load file="settings.conf"}{*
*}{include file="header.inc" menu="sections" title="Details of section" globuled=1 refreshing=3}
{literal}
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
{/literal}
{include file="footer.inc"}

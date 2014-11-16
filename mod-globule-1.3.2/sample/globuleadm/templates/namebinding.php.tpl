{config_load file="settings.conf"}{*
*}{include file="header.inc" menu="namebinding" title="NameBinding" globuled=1}
{literal}
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
{/literal}
{include file="footer.inc"}

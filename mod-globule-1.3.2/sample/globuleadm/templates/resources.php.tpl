{config_load file="settings.conf"}{*
*}{include file="header.inc" menu="general" title="General Information" globuled=1}
{literal}
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

{/literal}
{include file="footer.inc"}

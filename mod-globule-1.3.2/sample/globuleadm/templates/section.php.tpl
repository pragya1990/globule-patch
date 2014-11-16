{config_load file="settings.conf"}{*
*}{include file="header.inc" menu="sections" title="Details of section" globuled=1 refreshing=180}
{literal}
  <p>
  This page shows more details on the Globule-enabled web-site
  section <?= $_REQUEST["section_uri"] ?>:
  </p><p>
      <table cellspace="3" cellpadding="3" border="0">
      <tr>
        <th colspan="3" align="left"><a href="<?= $_REQUEST["section_uri"] ?>"><?= $_REQUEST["section_uri"] ?></a></th>
      </tr><tr>
        <td rowspan="8">&nbsp;</td>
        <td>The role your server plays for this site is</td>
        <td align="center">:</td>
        <td>&nbsp;</td>
        <td><?= $_REQUEST["section_type"] ?>&nbsp;</td>
      </tr><tr>
        <td>The main ServerName for this site is</td>
        <td align="center">:</td>
        <td>&nbsp;</td>
        <td><?= $_REQUEST["section_servername"] ?>&nbsp;</td>
      </tr><tr>
        <td>Files are located at the path</td>
        <td align="center">:</td>
        <td>&nbsp;</td>
        <td><?= $_REQUEST["section_path"] ?>&nbsp;</td>
      </tr><tr>
        <td>Redirection using</td>
        <td align="center">:</td>
        <td align="right">&nbsp;<a href="#" onClick="javascript:void(self.pinned=true);" onmouseover="balloon('When making decisions, Globule can often use multiple strategies to come to a decisions.  These strategies are implemented by different policy algorithms.  Sometimes you can choose a specific policy for your needs, sometimes Globule tries to search for the best policy available.<p>Redirection policies determin which of the available replica server should handle a request from a browsing internet user.  Available policies are:<br>RR: clients will be redirected to servers hosting this site in a round robin fashion.<br>AS: the clients will be redirected to the nearest available server.<br>WRR: like RR, but certains servers are assigned more load than others.<br>BAS: like AS, but balanced the load when certain servers are constantly being redirected to.')"><img src="i.png" border="0" alt="[info]"></a></td>
        <td><?= (!strcasecmp($_REQUEST["section_redirectmode"],"dns") ? "DNS redirection<br>" : (!strcasecmp($_REQUEST["section_redirectmode"],"http") ? "Only HTTP redirection<br>" : (!strcasecmp($_REQUEST["section_redirectmode"],"both") ? "Both DNS and HTTP redirection??<br>" : (!strcasecmp($_REQUEST["section_redirectmode"],"none") ? "Redirection switched off<br>" : "")))) ?>
        <?= $_REQUEST["section_redirectpolicy"] ?><?= strcmp($_REQUEST["section_redirectpolicy"],"") ? "policy" : "" ?>
        <?= strcmp($_REQUEST["section_redirectttl"],"") ? "with ttl " : "" ?><?= $_REQUEST["section_redirectttl"] ?><?= strcmp($_REQUEST["section_redirectttl"],"") ? "seconds" : "" ?>
      </tr><tr>
        <td>The default replication policy in use</td>
        <td align="center">:</td>
        <td align="right">&nbsp;<a href="#" onClick="javascript:void(self.pinned=true);" onmouseover="balloon('When making decisions, Globule can often use multiple strategies to come to a decisions.  These strategies are implemented by different policy algorithms.  Sometimes you can choose a specific policy for your needs, sometimes Globule tries to search for the best policy available.<p>Replication policies determin how Globule creates copies of documents and how these copies are maintained.')"><img src="i.png" border="0" alt="[info]"></a></td>
        <td><?= $_REQUEST["section_replicatepolicy"] ?>&nbsp;</td>
      </tr><tr>
        <td>Availability check periodicity</td>
        <td align="center">:</td>
        <td align="right">&nbsp;<a href="#" onClick="javascript:void(self.pinned=true);" onmouseover="balloon('Globule periodically checks whether the servers that help your web-site are available.  How often this is done is determined by this refresh interval.')"><img src="i.png" border="0" alt="[info]"></a></td>
        <td><?= globuleFormatInterval($_REQUEST["section_refreshinterval"]) ?>&nbsp;</td>
      </tr><tr>
        <td>Reevaluate performance periodicity</td>
        <td align="center">:</td>
        <td align="right">&nbsp;<a href="#" onClick="javascript:void(self.pinned=true);" onmouseover="balloon('Globule periodically determins if the replication policy is still the right one to use for each of the documents on your web-site.  How often this is done is determined by the re-evaluate interval.')"><img src="i.png" border="0" alt="[info]"></a></td>
        <td><?= globuleFormatInterval($_REQUEST["section_reevaluateinterval"]*$_REQUEST["section_refreshinterval"]) ?>
	    <?php if($_REQUEST["section_reevaluateinterval"]>0) { ?> (next reevaluation after <?= globuleFormatInterval($_REQUEST["section_reevaluateafter"]*$_REQUEST["section_refreshinterval"]) ?>) <?php } ?>
            &nbsp;
          </td>
      </tr><tr>
<!--
        <td colspan="3">
            [ <a href="section.php?url=<?= $_REQUEST["section".$i."_uri"] ?>">refresh</a>
            | <a href="section.php?url=<?= $_REQUEST["section".$i."_uri"] ?>&action=flush">flush</a>
            ]<br/>
          </td>
      </tr><tr>
-->
        <td colspan="4">&nbsp;</td>
      </tr>
      </table>
    </p>
    <h3>Peers</h3>
    Here is the list of other servers taking part in hosting the above web
    site:
    <table class="infotable">
    <tr class="infotr">
      <th class="infoth">ServerName</th>
      <th class="infoth">Role</th>
      <th class="infoth">Available</th>
    </tr>
    <?PHP for($i=0; $i<$_REQUEST["section_npeers"]; $i++) { ?>
    <tr class="infotr<?= floor($i/3)%2+1 ?>">
      <td class="infotd"><a href="<?= $_REQUEST["section_peer".$i."_uri"] ?>"><?= $_REQUEST["section_peer".$i] ?>&nbsp;</td>
      <td class="infotd"><?= $_REQUEST["section_peer".$i."_type"] ?>&nbsp;</td>
      <td class="infotd"><?= ($_REQUEST["section_peer".$i."_available"]=="true"?"<font color=green>yes</font>":"<font color=red>no</font>") ?>&nbsp;</td>
    </tr>
    <?PHP } ?>
    </table></br>
    [ <a href="peers.php?url=<?= $_REQUEST["section_uri"] ?>">details</a>
    ]<br/></br></br>
    <h3>Documents</h3>
    The documents loaded in memory are:
    <table class="infotable">
    <tr class="infotr">
      <!-- <td>name</th> -->
      <th class="infoth">path</th>
      <th class="infoth">policy</th>
      <th class="infoth">content-type</th>
      <th class="infoth">last-modified</th>
      <th class="infoth">size</th>
    </tr>
    <?PHP for($i=0; $i<$_REQUEST["section_ndocs"]; $i++) { ?>
    <tr class="infotr<?= floor($i/3)%2+1 ?>">
      <!-- <td class="infotd"><?= $_REQUEST["section_doc".$i] ?>&nbsp;</td> -->
      <td class="infotd"><a href="<?= $_REQUEST["section_uri"] ?><?= $_REQUEST["section_doc".$i."_path"]?>" class="infoa"><?= $_REQUEST["section_doc".$i."_path"] ?></a>&nbsp;</td>
      <td class="infotd"><?= $_REQUEST["section_doc".$i."_policy"] ?>&nbsp;</td>
      <td class="infotd"><?= $_REQUEST["section_doc".$i."_contenttype"] ?>&nbsp;</td>
      <td class="infotd"><?= globuleFormatTime($_REQUEST["section_doc".$i."_lastmod"]) ?>&nbsp;</td>
      <td class="infotd"><?= globuleFormatFileSize($_REQUEST["section_doc".$i."_docsize"]) ?>&nbsp;</td>
    </tr>
    <?PHP } ?>
    </table>
    <p>[ <a href="section-report.php?url=<?= $_REQUEST["section_uri"] ?>">usage statistics by webalizer</a>
    ]
    </p>
{/literal}
{include file="footer.inc"}

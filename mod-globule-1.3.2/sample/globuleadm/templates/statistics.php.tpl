{config_load file="settings.conf"}{*
*}{include file="header.inc" menu="sections" title="Statistics" globuled=1}
{literal}
    <?php globuleGetInfo("hbcount=heartbeat"); ?>
    Number of heartbeats: <?= $ARGS['hbcount'] ?>
{/literal}
{include file="footer.inc"}

{config_load file="settings.conf"}{*
*}{include file="header.inc" menu="index" title="Index"}
  Welcome to the administration interface of server
  <b><code><?=$_SERVER["SERVER_NAME"]?></code></b> owned by
  <a href="mailto:<?=$_SERVER["SERVER_ADMIN"]?>"><?=$_SERVER["SERVER_ADMIN"]
  ?></a>.
  </p><p>
  This web page allows you to view, monitor and alter certain
  statistics and settings of this server.
  </p><p>
  On the top of this web-page is a menu to the different items available.
  Browse through them to see what's available.  When on a particular web-page,
  you may hoover with your mouse over items to get an explanation.
  </p><p>
  For a general explanation about <a href="http://www.globule.org/">Globule</a>
  and its configuration, please read the
  <a href="http://www.globule.org/docs/">user manual</a>.
  </p><p>
{include file="footer.inc"}

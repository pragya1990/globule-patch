{config_load file="settings.conf"}{*
*}{literal}<?PHP
$rtncode = 0;
error_reporting(error_reporting() & ~E_WARNING);
if(!($stat = stat($_REQUEST["section_path"]."/.htglobule/report/index.html"))
   || $stat['mtime']+3600 < time())
  $rtnline = exec(dirname($_ENV["_"]) . "/../bin/globulectl 2>&1 webalizer " .
                  $_REQUEST["section_path"] . " " .
                  $_REQUEST["section_servername"], $output, $rtncode);
if($rtncode == 0) {
  header("Location: " . $_REQUEST["section_uri"] . "/.htglobule/report/");
  exit;
}
?>
{/literal}{*
*}{include file="header.inc" menu="sections" title="Section report"}
{literal}
  <center>
  <font color="red">site error</font>
  </center>
  </p><p>
  An error occurred when trying to re-generate the report (because it was
  either too old or non-existent).  Check the server logs for more details
  all I got until now it the error code <?= $rtncode ?> and the
  message:
  </p>
  <pre><?= implode($output,"\n") ?></pre>
  <p>
  Sorry about this.
{/literal}
{include file="footer.inc"}

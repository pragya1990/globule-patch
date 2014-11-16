{config_load file="settings.conf"}{*
*}{include file="header.inc" menu="diagnostics" title="Diagnostics" globuled=1}
{literal}
    <p>

    As part of its operation, Globule writes messages into the error-log when
    it encounters certain circumstances that may be of interest to its
    administrator.
    Besides being written into the log files, the latest messages can also be
    reviewed through this web-interface.

    The messages are classified depending on their seriousness,
    such as plain informational, warnings, errors, and critical messages
    and so on.  You can select through which extend you want to have
    these messages logged:
    </p>
    <form action="diagnostics.php" METHOD="POST" ENCTYPE="application/x-www-form-urlencoded">
      <select name="profile">
        <option selected>Choose one</option>
        <option>normal</option>
        <option>extended</option>
        <option>verbose</option>
      </select>
      <input type="SUBMIT" name="SUBMIT" value="select">
    </form>
    <h3>Error reporting</h3>
    <hr class="subsectionhr"/>
    <p>
      Latest error messages:
      <?PHP globuleGetInfo("msgs=x2"); ?>
      <blockquote><pre><?PHP
        passthru('cd ../.. ; if [ -r logs/error.log -a logs/error.log -nt etc/httpd.conf ]; then tail -30 logs/error.log ; else echo "Error file not accessible" ; fi');
      ?></pre></blockquote>
    </p>
{/literal}
{include file="footer.inc"}

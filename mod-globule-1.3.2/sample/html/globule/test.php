<?PHP eval(stripslashes($_SERVER["GLOBULE_PHPSCRIPT"])); ?>
<html><head>
  <title>PHP test Page for Apache/Globule Installation</title>
  <link href="Globule.css" rel="stylesheet" title="preferred" />
</head><body>
  <table class="pagemain"><tr><td>
    <h1>Welcome to Apache/Globule</h1>
    <hr class="titlehr"/>
    <h2>PHP sanity test</h2>
    <hr class="sectionhr"/>
    <p>
    If PHP replicated pages are working correctly, there should be
    no errors below, even when the page is being requested from a
    replica server.
    </p>
    <blockquote><pre>
Inclusion of sub-php page and global variables:   <?PHP $test=1; require globule("test_sub.php"); ?><?PHP ++$test; if($test != 3) echo ( "FAILED!" ); else echo "OK." ?>
    </pre></blockquote>
  <br/><br/><br/><br/><br/><br/>
  </td></tr></table>
</body></html>

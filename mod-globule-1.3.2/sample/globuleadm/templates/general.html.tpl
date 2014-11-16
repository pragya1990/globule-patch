<html><head>
  <title>Globule Controls</title>
  <link rel="SHORTCUT ICON" href="http://www.globule.org/favicon.ico" />
  <link href="Globule.css" rel="stylesheet" title="preferred" />
</head><body>
  <center>
  [ <a class="topnav" href="index.html">index</a>
  | <a class="topnav" href="general.html"><b>general</b></a>
  | <a class="topnav" href="diagnostics.html">diagnostics</a>
  ]
  </center>
  <table class="pagemain"><tr><td>
    <h1>Globule Controls</h1>
    <hr class="titlehr"/>
    <h2>General Information</h2>
    <hr class="sectionhr"/>
{literal}
    <hr class="sectionhr"/>
    <p>
    This page shows generic information about the software and machine the
    Globule-enabled web-server.
    </p><p>
    This Globule-enabled web-server is running the following software and
    configuration settings:
    </p><p>
      <form action="general.php" METHOD="POST" ENCTYPE="application/x-www-form-urlencoded">

      <table cellspacing="3" cellpadding="0" border="0"><tr>
        <td>PHP support</td>
        <td align="right">&nbsp;<a href="#" onClick="javascript:void(self.pinned=true);" onmouseover="balloon('PHP allows a web-site builder to use dynamic content.  Pages where the content is customized per request, such as displaying the results of a search query, forums discussions, user preferences, and so on.')"><img src="i.png" border="0" alt="[info]"></a></td>
        <td align="center">:</td>
        <td><!-- <?php echo "--", ">", "enabled", "<", "!--"; // -->disabled; most information will not be available<!-- ?>
	  -->
          </td>
      </tr><tr>
        <td>Globule version</td>
        <td align="right">&nbsp;<a href="#" onClick="javascript:void(self.pinned=true);" onmouseover="balloon('It is always recommended that you use the latest version.  Up-to-date versions are available <a href=http://www.globule.org/download/>here</a>.  Also, please comunicate this version number to us if you need help about Globule.')"><img src="i.png" border="0" alt="[info]"></a></td>
        <td align="center">:</td>
        <td><?= $_REQUEST['version'] ?></td>
      </tr><tr>
        <td>Apache version</td>
        <td align="right">&nbsp;<a href="#" onClick="javascript:void(self.pinned=true);" onmouseover="balloon('Globule is running on Apache software.  You automatically keep up-to-date with the latest Apache software by updating your Globule version when using the installer.')"><img src="i.png" border="0" alt="[info]"></a></td>
        <td align="center">:</td>
        <td><?= $_SERVER["SERVER_SOFTWARE"] ?></td>
      </tr><tr>
        <td>Broker serial</td>
        <td align="right">&nbsp;<a href="#" onClick="javascript:void(self.pinned=true);" onmouseover="balloon('If your server is registered with the GBS, then the GBS will generate config files automatically.  To make sure that your server uses the latest generated version of its config file, check that this serial num,ber is identical to the one mentioned by the GBS.  If it is not, then you should restart your server.')"><img src="i.png" border="0" alt="[info]"></a></td>
        <td align="center">:</td>
        <td><?= array_key_exists('gbs',$_REQUEST) && strcmp($_REQUEST['gbs'],"") ? $_REQUEST['gbs'] : "server does not use the Globule Broker System" ?></td>
      <!--
      </tr><tr>
        <td>Test string</td>
        <td align="right">&nbsp;<a href="#" onClick="javascript:void(self.pinned=true);" onmouseover="balloon('For testing purposes only')"><img src="i.png" border="0" alt="[info]"></a></td>
        <td align="center">:</td>
        <td><script language="php">
            echo ( "<input type=\"text\" name=\"test\" value=\"" . $_REQUEST['test'] . "\"/>" );
	    </script>&nbsp;
          </td>
      -->
      </tr></table>
      </form>
    <!--
    </p><p>
      This web-server is running on the machine host named world.cs.vu.nl
      under the administrative control of berry@cs.vu.nl.  From this
      machine, the Globule software uses certain resources, such as memory,
      diskspace.
    </p><p>
      <table cellspacing="3" cellpadding="0" border="0"><tr>
      </tr><tr>
        <td>Shared memory usage</td>
        <td align="right">&nbsp;<a href="#" onClick="javascript:void(self.pinned=true);" onmouseover="balloon('Of the total amount of memory, a small piece is assigned to hold shared administrative data.  Below is the amount of this piece used, which should never get filled upto maximum capacity otherwise the web-site will seize functioning.  Note that this shared memory is not the same as your real memory in your computer, just the piece assigned to Globule.')"><img src="i.png" border="0" alt="[info]"></a></td>
        <td align="center">:</td>
        <td><?=round(($_REQUEST['shmbytes']+$_REQUEST['shmitems']*8)*100/$_REQUEST['shmsize'],1) ?>%; approximately <?= globuleFormatFileSize($_REQUEST['shmbytes']+$_REQUEST['shmitems']*8) ?> of <?= globuleFormatFileSize($_REQUEST['shmsize']) ?> in <?= $_REQUEST['shmitems'] ?> items</td>
      </tr></table>
    </p>
    <center>
      <img src="chart.php?black=t+v+shm2" alt="shared memory usage">
    </center>
    -->
{/literal}
    </p>
  <br/><br/><br/><br/><br/><br/>
  </td></tr></table>
</body></html>

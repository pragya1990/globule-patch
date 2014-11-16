<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
        "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html><head>
  <title>Globule Broker System</title>
  <meta http-equiv="Content-Type" content="text/html; charset=ISO-8859-1" />
  <link rel="stylesheet" href="gbs.css" type="text/css" />
  <link rel="SHORTCUT ICON" href="http://www.globule.org/favicon.ico">
</head><body>
  <div id="topImage">
    <img src="images/globsm.jpg" alt="GBS logo" align="top"/><span class="Logo">Globule Broker System</span><span class="Title">: {$title}</span>
  </div>
  <div id="userName">
    <div id="loginStatus">
{php_supported}
      <form>
        <input type="text" name="username" value=""><br>
        <input type="password" name="password" value="">
      </form>
{php_unsupported}
      no login available
{/php_supported}
    </div>
    {$header}
  </div>
  <div id="leftOptions">
{php_supported}
    <a href="./">index</a><br>
{php_unsupported}
    &nbsp;
{/php_supported}
  </div>
  <div id="mainPage">
    {$content}
  </div>
  <div id="footer"><address>berry@cs.vu.nl</address></div>
</body></html>

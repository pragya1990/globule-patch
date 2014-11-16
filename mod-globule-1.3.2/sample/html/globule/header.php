<html><head>
  <title><?PHP echo ( $page['title'] ); ?></title>
  <link href="Globule.css" rel="stylesheet" title="preferred" />
  <script language="javascript" />
  var iters;
  function showGlobuleIter() {
    var displaydelay = 500;
    var globuleframe = document.getElementById('globule');
    if(iters < 0) {
      globuleframe.style.left = -globuleframe.clientWidth;
      globuleframe.style.top  = window.innerHeight - globuleframe.clientHeight;
      globuleframe.style.visibility = 'visible';
      iters = displaydelay+2*globuleframe.clientWidth;
      timerID = self.setTimeout("showGlobuleIter()", 1);
    } else if(iters > 0) {
      var pos;
      iters -= 3;
      if(iters < 0)
        iters = 0;
      if(iters > globuleframe.clientWidth+displaydelay)
        globuleframe.style.left = -(iters - globuleframe.clientWidth - displaydelay);
      else if(iters < globuleframe.clientWidth)
        globuleframe.style.left = iters - globuleframe.clientWidth;
      timerID = self.setTimeout("showGlobuleIter()", 1);
    } else
      clearTimeout(timerID);
  }
  function showGlobule() {
    iters = -1;
    timerID = self.setTimeout("showGlobuleIter()", 1000);
  }
  </script>
</head><body onLoad="showGlobule()">
<div id="globule" style="visibility:hidden;position:absolute;left:0px;top:0px;z-index:100"><iframe allowtransparency="true" frameborder="0" src="site.html"></iframe></div>
  <table class="pagemain"><tr><td>
    <img src="globule.png" align="right" />
    <h1><?PHP echo ( $page['header'] ); ?></h1>
    <hr class="titlehr"/>

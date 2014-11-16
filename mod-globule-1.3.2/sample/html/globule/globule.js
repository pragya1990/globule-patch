var iters;
function showGlobuleIter() {
  var displaydelay = 1500;
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

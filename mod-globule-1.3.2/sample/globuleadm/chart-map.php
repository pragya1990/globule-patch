<?PHP require_once("globule.php");

$width  = key_exists('width',$_REQUEST)  ? $_REQUEST['width']  : 512;
$height = key_exists('height',$_REQUEST) ? $_REQUEST['height'] : 512;

function globuleImageText($s, $font = 3)
{
  global $width, $height, $image, $black;
  $font = 3;
  $x = $width/2  - imageFontWidth($font)*strlen($s)/2;
  $y = $height/2 - imageFontHeight($font)/2;
  imageFontWidth($font);
  imageString($image, $font, $x, $y, $s, $black);
}

$data = array();
if($_REQUEST['query'])
  $query = $_REQUEST['query'] . "&";
else
  $query = "";
$query .= "black=info+" . urlencode($_REQUEST['black']);
globuleInfo($data,$query);

$minx =  9223372036854775807;
$miny =  9223372036854775807;
$maxx = -9223372036854775808;
$maxy = -9223372036854775808;

foreach($data as $x => $y) {
#echo $x . "\t" . $y . "\n";
  if($x    < $minx)  $minx = $x;
  if($x+$y > $maxx)  $maxx = $x+$y;
}

$image = imageCreate($width,$height);
imageFilledRectangle($image,0,0,$width,$height,$white);
$white = imageColorAllocate($image, 0xff, 0xff, 0xff);
$black = imageColorAllocate($image, 0x00, 0x00, 0x00);

$delta = ($maxx - $minx) / ($width * $height);
foreach($data as $x => $y) {
  $x1 = (($x-$minx)/$delta)%$width;
  $y1 = round((($x-$minx)/$delta)/$width,0);
  $x2 = (($x+$y-$minx)/$delta)%$width;
  $y2 = round((($x+$y-$minx)/$delta)/$width,0);
  if($y2 > $y1) {
    imageLine($image, $x1, $y1, $width-1, $y1, $black);
    if($y2 - $y1 > 1)
      imageFilledRectangle($image,0,$y1+1,$width-1,$y2-1,$black);
    imageLine($image, 0, $y2, $x2, $y2, $black);
  } else
    imageLine($image, $x1, $y1, $x2, $y2, $black);
}

imageLine($image, 0,        0,         0,        $height,   $black);
imageLine($image, $width-1, 0,         $width-1, $height,   $black);
imageLine($image, 0,        0,         $width,   0,         $black);
imageLine($image, 0,        $height-1, $width,   $height-1, $black);

header("Content-type: image/png");
imagePNG($image);
imageDestroy($image);

?>

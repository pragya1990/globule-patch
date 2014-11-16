<?PHP require_once("globule.php");

$width  = key_exists('width',$_REQUEST)  ? $_REQUEST['width']  : 500;
$height = key_exists('height',$_REQUEST) ? $_REQUEST['height'] : 250;

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
globuleInfo($data,"black=info+" . urlencode($_REQUEST['black']));
$minx =  9223372036854775807;
$miny =  9223372036854775807;
$maxx = -9223372036854775808;
$maxy = -9223372036854775808;

foreach($data as $x => $y) {
  if($x < $minx)  $minx = $x;
  if($x > $maxx)  $maxx = $x;
  if($y < $miny)  $miny = $y;
  if($y > $maxy)  $maxy = $y;
}
$dx =   ($width-1)  / ($maxx - $minx);
$dy = - ($height-1) / ($maxy - $miny);
$ncoods = 2;
$coords = array( $width-1, $height-1, 0, $height-1 );
foreach($data as $x => $y) {
  $coords[] = round(($x - $minx) * $dx);
  $coords[] = round(($y - $miny) * $dy + $height-1);
  ++$ncoords;
}

$image = imageCreate($width,$height);
$white = imageColorAllocate($image, 0xff, 0xff, 0xff);
$black = imageColorAllocate($image, 0x00, 0x00, 0x00);

imageFilledRectangle($image,0,0,$width,$height,$white);
imageLine($image, 0,        0,         0,        $height,   $black);
imageLine($image, $width-1, 0,         $width-1, $height,   $black);
imageLine($image, 0,        0,         $width,   0,         $black);
imageLine($image, 0,        $height-1, $width,   $height-1, $black);

if($ncoords >= 4) {
  imagefilledpolygon($image, $coords, $ncoords, $black);
} else
  globuleImageText("No data available");

header("Content-type: image/png");
imagePNG($image);
imageDestroy($image);

?>

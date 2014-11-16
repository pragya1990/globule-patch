<?PHP
require_once("Smarty.class.php");
$smarty->caching = false;
$smarty = new Smarty();
$smarty_block_page_set=0;

function smarty_function_php_unsupported($param,&$smarty)
{
  return "<?php // -->";
}
function smarty_block_php_supported($param,$content,&$smarty,&$repeat)
{
  if(isset($content))
    return $content . "<!-- ?> -->";
  else
    return "<!-- <?php echo \" --\", \">\" ?>";
}
function smarty_block_page($params,$content,&$smarty,&$repeat)
{
  if(isset($content))
    return $smarty->fetch($params['template'] . ".tpl");
  return "";
}
function smarty_block_pagecontent($params,$content,&$smarty,&$repeat)
{
  if(isset($content))
    $smarty->assign('content',$content);
  return "";
}
$smarty->register_block('page','smarty_block_page');
$smarty->register_block('pagecontent','smarty_block_pagecontent');
$smarty->register_block('php_supported','smarty_block_php_supported');
$smarty->register_function('php_unsupported','smarty_function_php_unsupported');
if(substr($_SERVER["REQUEST_URI"],strlen($_SERVER["REQUEST_URI"])-1 )=="/")
  $smarty->display("index.tpl");
else
  $smarty->display(basename($_SERVER["REQUEST_URI"],".php").".tpl");
?>

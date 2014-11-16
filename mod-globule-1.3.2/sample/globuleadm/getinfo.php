<?PHP
  foreach($_REQUEST as $key => $val)
    echo '$ARGS[\'', $key, "']='", $val, "';\n";
?>

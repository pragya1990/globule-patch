<?PHP

function globule ($fname) {
  $item = $fname;
  if(!strncmp($fname,"/",1)) {
    if(strncmp($fname,$_SERVER['DOCUMENT_ROOT'],
               strlen($_SERVER['DOCUMENT_ROOT'])))
    {
      $item  = $fname;
      $fname = $_SERVER['DOCUMENT_ROOT'] . $fname;
    } else
      $item = substr($fname,strlen($_SERVER['DOCUMENT_ROOT'])+1);
  } else
    $item = $fname;

  while(!strncmp($item,"/",1))
    $item = substr($item,1);
  $url = parse_url($_SERVER['GLOBULE_SECTION'] . "/" . $item);
  $host = $url["host"];
  $port = $url["port"];
  $file = $url["path"];
  if($port == 0) {
    $servername = $host;
    $port = 80;
  } else
    $servername = $host . ":" . $port;
  $fp = fsockopen("tcp://127.0.0.1", $port);
  fwrite($fp, "HEAD " . $file . " HTTP/1.1\r\nHost: " . $servername .
         "\r\nConnection: Close\r\n\r\n");
  while(!feof($fp)) {
    fgets($fp, 128);
  }
  fclose($fp);
  return $fname;
}

function globuleGetInfo ($keys) {
  global $ARGS;
  # for kv in $keys make k=v&k=v, etc
  $url = parse_url("http://" . $_SERVER['SERVER_NAME'] . ":"
       . $_SERVER['SERVER_PORT'] . dirname($_SERVER['REQUEST_URI'])
       . "/getinfo.php?" . $keys);
  $host = $url["host"];
  $port = $url["port"];
  if($port == 0) {
    $servername = $host;
    $port = 80;
  } else
    $servername = $host . ":" . $port;
  $fp = fsockopen("tcp://127.0.0.1", $port);
  fwrite($fp, "GET " . $url['path'] . "?" . $url['query'] .
         " HTTP/1.1\r\nHost: " . $servername . "\r\nConnection: Close\r\n\r\n");
  if(!feof($fp) && sscanf(fgets($fp),"HTTP/1.1 %d %s",$status,$message) > 0 &&
     $status == 200)
  {
    $result = "";
    $chunked = 0;
    while(!feof($fp)) {
      $s = fgets($fp);
      if($s == "" || $s == "\n\n" || $s == "\r\n" )
        break;
      if(preg_match('/Transfer\\-Encoding:\\s+chunked\\r\\n/',$s))
        $chunked = 1;
    }
    if($chunked) {
      do {
        $ch = '';
	$chunksize = '';
	do {
	  $chunksize .= $ch;
	  $ch = fread($fp,1);
	} while($ch != "\r" && $ch != "\n"); // till match CR
	if($ch == "\r")
	  fread($fp,1); // skip the LF
	$chunksize = hexdec($chunksize);
	if($chunksize > 0) {
	  $result .= fread($fp,$chunksize);
	  fread($fp,2); // discard the CRLF trailing chunk
        }
      } while($chunksize);
    } else {
      while(!feof($fp)) {
        $s = fgets($fp);
        $result .= $s;
      }
    }
  }
  fclose($fp);
  eval($result);
}

function globuleInfo (&$var, $keys) {
  # dirname($_SERVER['REQUEST_URI') in case relative URL?
  $var = array();
  $url = parse_url("http://" . $_SERVER['SERVER_NAME'] . ":"
       . $_SERVER['SERVER_PORT'] . dirname($_SERVER['SCRIPT_NAME'])
       . "/info?" . $keys);
  $host = $url["host"];
  $port = $url["port"];
  if($port == 0) {
    $servername = $host;
    $port = 80;
  } else
    $servername = $host . ":" . $port;
  $fp = fsockopen("tcp://127.0.0.1", $port);
  fwrite($fp, "GET " .$url['path'] . "?" . $url['query'] .
         " HTTP/1.1\r\nHost: " . $servername . "\r\nConnection: Close\r\n\r\n");
  if(!feof($fp) && sscanf(fgets($fp),"HTTP/1.1 %d %s",$status,$message) > 0 &&
     $status == 200)
  {
    while(!feof($fp)) {
      $s = fgets($fp);
      if($s == "" || $s == "\n\n" || $s == "\r\n" )
      break;
    }
    while(!feof($fp)) {
      $s = fgets($fp);
      parse_str($s,$var);
    }
  }
  fclose($fp);
}

function globuleFormatFileSize ($bytes) {
  if($bytes > 2*1048576)
    return number_format($bytes/1048576,1,".","") . " MB";
  else if($bytes > 2*1024)
    return number_format($bytes/1024,1,".","") . " KB";
  else
    return $bytes;
}

function globuleFormatTime ($timestamp) {
  return gmstrftime("%A %d-%b-%y %T %Z",$timestamp); 
}

function globuleFormatInterval ($seconds) {
  $rtstring = "";
  $days    = floor($seconds / 86400);
  $hours   = floor($seconds / 3600) % 24;
  $minutes = floor($seconds / 60) % 60;
  $seconds = $seconds % 60;
  if($days > 0) {
    if(strcmp($rtstring,""))
      $rtstring .= " ";
    if($days > 1)
      $rtstring .= $days . " days";
    else if($hours==0 && $minutes==0 && $seconds==0)
      $rtstring .= "daily";
    else
      $rtstring .= "one day";
  }
  if($hours > 0) {
    if(strcmp($rtstring,""))
      $rtstring .= " ";
    if($hours > 1)
      $rtstring .= $hours . " hours";
    else if($days==0 && $minutes==0 && $seconds==0)
      $rtstring .= "every hour";
    else
      $rtstring .= $hours . " hours";
  }
  if($minutes > 0) {
    if(strcmp($rtstring,""))
      $rtstring .= " ";
    if($minutes > 1)
      $rtstring .= $minutes . " minutes";
    else
      $rtstring .= "one minut";
  }
  if($seconds > 0) {
    if(strcmp($rtstring,""))
      $rtstring .= " ";
    if($seconds > 1)
      $rtstring .= $seconds . " seconds";
    else
      $rtstring .= "one second";
  }
  if(strcmp($rtstring,""))
    return $rtstring;
  else
    return "never";
}

$globule_mysql__links = array();
$globule_mysql__statements = array();
$globule_mysql__keywords = array (
  array("SELECT",  "GET"),
  array("UPDATE",  "POST"),
  array("INSERT",  "POST"),
  array("REPLACE", "POST"),
  array("DELETE",  "POST")
);


function globule_mysql__reconnect($link)
{
  global $globule_mysql__links;
  $fp = fsockopen("tcp://127.0.0.1", $_SERVER['SERVER_PORT']);
  $globule_mysql__links[$link-1]['fp'] = $fp;
}

function globule_mysql_attach($location, $new_link = FALSE, $client_flags = 0)
{
  global $globule_mysql__links;
  $link = array ( 'location' => $location,
                  'errno'    => 0,
                  'error'     => "",
                  'affected' => -1 );
  $globule_mysql__links[] = $link;
  $link = count($globule_mysql__links);
  globule_mysql__reconnect($link);
  return $link;
}

function globule_mysql_reattach($location, $link = 0)
{
  global $globule_mysql__links;
  if($link == 0)
    $link = count($globule_mysql__links);
  $globule_mysql__links[$link-1]['location'] = $location;
  $globule_mysql__links[$link-1]['errno']    = 0;
  $globule_mysql__links[$link-1]['error']    = "";
  return TRUE;
}

function globule_mysql_connect($hostname = "",
                               $username = "",
                               $password = "",
                               $new_link = FALSE, $client_flags = 0)
{
  global $globule_mysql__links;
  $link = array ( 'hostname' => $hostname,
                  'username' => $username,
                  'password' => $password,
                  'errno'    => -1,
                  'error'    => "not attached to globule",
                  'affected' => -1 );
  $globule_mysql__links[] = $link;
  $link = count($globule_mysql__links);
  globule_mysql__reconnect($link);
  return $link;
}

function globule_pconnect($hostname = "",
                          $username = "",
                          $password = "",
                          $client_flags = 0)
{
  global $globule_mysql__links;
  return FALSE;
}

function globule_mysql_close($link = 0)
{
  global $globule_mysql__links;
  if($link == 0)
    $link = count($globule_mysql__links);
  fclose($globule_mysql__links[$link-1]['fp']);
  return TRUE;
}

function globule_mysql_select_db($database, $link = 0)
{
  global $globule_mysql__links;
  if($link == 0)
    $link = count($globule_mysql__links);
  $globule_mysql__links[$link-1]['database'] = $database;
  return TRUE;
}

function globule_mysql__getcontent($fp)
{
  $content = "";
  $chunked = 0;
  $length = -1;
  while(!feof($fp)) {
    $s = fgets($fp);
    if($s == "" || $s == "\n\n" || $s == "\r\n" )
      break;
    if(preg_match('/Transfer\\-Encoding:\\s+chunked\\r\\n/',$s))
      $chunked = 1;
    $args = array();
    if(preg_match('/Content-Length:\\s+(\\d+)\\r\\n/',$s,$args)) {
      $length = $args[0];
    }
  }
  if($chunked) {
    do {
      $ch = '';
      $chunksize = '';
      do {
        $chunksize .= $ch;
        $ch = fread($fp,1);
      } while($ch != "\r" && $ch != "\n"); // till match CR
      if($ch == "\r")
        fread($fp,1); // skip the LF
      $chunksize = hexdec($chunksize);
      if($chunksize > 0)
        $content .= fread($fp,$chunksize);
      fread($fp,2); // discard the CRLF trailing chunk
    } while($chunksize);
  } else if($length >= 0) {
    if($length > 0)
      $content = fread($fp,$length);
  } else {
    while(!feof($fp)) {
      $s = fgets($fp);
      $content .= $s;
    }
  }
  return $content;
}

function globule_mysql__declare($link, $qname, $method, $qstmt, $deps)
{
  global $globule_mysql__links, $globule_mysql__statements;
  if($link == 0)
    $link = count($globule_mysql__links);

  $location = $globule_mysql__links[$link-1]['location'];
  if($location == "")
    $location = $globule_mysql__links[$link-1]['database'];
  if(strncmp($location,"/",1))
    $location = "/" . $location;

  $url = parse_url("http://" . $_SERVER['SERVER_NAME'] . ":"
       . $_SERVER['SERVER_PORT'] . $location . "/" . $qname);
  $host = $url["host"];
  $port = $url["port"];
  if($port == 0) {
    $servername = $host;
    $port = 80;
  } else
    $servername = $host . ":" . $port;

  $pattern = "";
  foreach(str_split($qstmt) as $ch)
    switch($ch) {
    case "?":
      $pattern[] = "(.*)";
      break;
    case "\\":
    case "^":
    case "$":
    case ".":
    case "|":
    case "(":
    case ")":
    case "[":
    case "]":
    case "*":
    case "+":
    case "?":
    case "{":
    case "}":
      $pattern[] = "\\" . $ch;
      break;
    default:
      $pattern[] = $ch;
    }
  $pattern = "/^" . implode($pattern) . "$/";
  $globule_mysql__statements[$qname] = array (
             'statement' => $qstmt,
             'pattern'   => $pattern,
             'method'    => $method );

  $query = $qstmt . "\n" . implode("\n", $deps) . "\n";
  $query = "PUT " . $url['path']  . " HTTP/1.1\r\nHost: " . $servername
         . "\r\nConnection: keep-alive\r\nContent-length: "
	 . (strlen($query)) . "\r\n\r\n" . $query;
  $fp = $globule_mysql__links[$link-1]['fp'];
  if(fwrite($fp,$query) == FALSE) {
    globule_mysql__reconnect($link);
    $fp = $globule_mysql__links[$link-1]['fp'];
    if(fwrite($fp,$query) == FALSE)
      return FALSE;
  }
  if(!feof($fp) && sscanf(fgets($fp),"HTTP/1.1 %d %s",$status,$message) > 0) {
    if($status == 201) {
      globule_mysql__getcontent($fp);
    } else {
      fclose($fp);
    }
  }
}

function globule_mysql_declare($qname, $qstmt, $deps = "", $link = 0)
{
  global $globule_mysql__links, $globule_mysql__keywords;
  if(!is_array($deps) && is_numeric($deps) && $link == 0)
    $link = $deps;
  if($link == 0)
    $link = count($globule_mysql__links);
  foreach($globule_mysql__keywords as $keyword)
    if(preg_match('/^[ \t]*' . $keyword[0] . '.*$/', $qstmt)) {
      $method = $keyword[1];
      if(!is_array($deps))
        if($keyword[1] == "GET") {
          $deps = array ( $qname );
        } else {
          $deps = array ( "" );
        }
      break;
    }
  return globule_mysql__declare($link, $qname, $method, $qstmt, $deps);
}

function& globule_mysql__query($link, $qname, $args, $method)
{
  global $globule_mysql__links, $globule_mysql__statements;
  if($link == 0)
    $link = count($globule_mysql__links);
  $fp = $globule_mysql__links[$link-1]['fp'];

  $location = $globule_mysql__links[$link-1]['location'];
  if($location == "")
    $location = $globule_mysql__links[$link-1]['database'];
  if(strncmp($location,"/",1))
    $location = "/" . $location;

  $url = parse_url("http://" . $_SERVER['SERVER_NAME'] . ":"
       . $_SERVER['SERVER_PORT'] . $location . "/" . $qname );
  $host = $url["host"];
  $port = $url["port"];
  if($port == 0) {
    $servername = $host;
    $port = 80;
  } else
    $servername = $host . ":" . $port;

  if(count($args) > 0)
    $query = implode("\n", $args) . "\n";

  $query = $method . " " . $url['path'] . " HTTP/1.1\r\nHost: " . $servername
         . "\r\nConnection: keep-alive\r\nContent-length: "
	 . (strlen($query)) . "\r\n\r\n" . $query;
  $fp = $globule_mysql__links[$link-1]['fp'];
  if(fwrite($fp,$query) == FALSE) {
    globule_mysql__reconnect($link);
    $fp = $globule_mysql__links[$link-1]['fp'];
    if(fwrite($fp,$query) == FALSE)
      return FALSE;
  }
  if(!feof($fp) && sscanf(fgets($fp),"HTTP/1.1 %d %s",$status,$message) > 0 &&
     $status == 200)
  {
    $content = globule_mysql__getcontent($fp);
    $result = array();
    foreach(explode("\n",$content) as $row)
      $result[]  = explode("\t",$row);
    $result = array ( 0, $result );
    # $result = array ( 0, explode("\n",$content) );
    return $result;
  } else
    return FALSE;
}

function& globule_mysql_execute($qname, $args = array(), $link = 0)
{
  global $globule_mysql__statements;
  return globule_mysql__query($link, $qname, $args,
                              $globule_mysql__statements[$qname]['method']);
}

function& globule_mysql_query($query, $link = 0)
{
  global $globule_mysql__links, $globule_mysql__statements;
  foreach($globule_mysql__statements as $qname => $statement) {
    $matches = array();
    if(preg_match($statement['pattern'],$query,$matches)) {
      return globule_mysql__query($link,$qname,$matches,$statement['method']);
    }
  }
  return globule_mysql_execute($qname, array(), $link);
}

function globule_mysql_fetch_row(&$res)
{
  ++$res[0];
  if($res[0] < count($res[1])) {
    return $res[1][$res[0]-1];
  } else
    return FALSE;
}

function globule_mysql_errno($link = 0)
{
  global $globule_mysql__links;
  if($link == 0)
    $link = count($globule_mysql__links);
  return $globule_mysql__links[$link-1]['errno'];
}

function globule_mysql_error($link = 0)
{
  global $globule_mysql__links;
  if($link == 0)
    $link = count($globule_mysql__links);
  return $globule_mysql__links[$link-1]['error'];
}

function globule_mysql_affected_rows($link = 0)
{
  global $globule_mysql__links;
  if($link == 0)
    $link = count($globule_mysql__links);
  return $globule_mysql__links[$link-1]['affected'];
}

function globule_mysql_change_user($user, $string, $password,
                                   $database = "", $link = 0)
{
  die("call of depricated function");
}

function globule_mysql_data_seek(&$res, $row)
{
  $res[0] = $row;
  if($res[0]+1 < count($res[1])) {
    return TRUE;
  } else
    return FALSE;
}

function globule_mysql_ping($link = 0)
{
  return TRUE;
}

?>

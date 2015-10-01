<?php header('Content-type: text/html; charset=utf-8'); ?>
<!DOCTYPE html>
<html>
  <head>
    <link rel="stylesheet" type="text/css" href="/~USERNAME/markdown/style.css"/>
    <meta name="content-type" http-equiv="content-type" value="text/html; utf-8"/>
    <script src="/jsMath/easy/load.js"></script>
  </head>
  <body>
<?php

require('markdown.php');

$legalExtensions = array('md', 'markdown');

$file = realpath($_SERVER['PATH_TRANSLATED']);
if($file &&
   in_array(strtolower(substr($file, strrpos($file, '.') + 1)),
	    $legalExtensions)) {
  echo Markdown(file_get_contents($file));
}
else {
  echo "<p>Bad filename given</p>";
}
?>
  </body>
</html>

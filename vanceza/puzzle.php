<html>
<head><title>Puzzle Page</title></head>
<body>
You are shown four cards. You know that each card definitely has a letter on one side and a number on the other.
<br />
However, somebody you don't trust also tells you that if any of these cards has a <b>D</b> on one side, it also has a <b>3</b> on the other.
<br />
The four cards show: <b>F 3 D 7</b>
<br />
What is the least number of cards you would need to turn over to find out whether you were being told the truth, and which ones are they? 
<?php
$fp=fopen('accessed.txt','w');
fwrite($fp,'1');
fclose($fp);
?>
</body>
</html>
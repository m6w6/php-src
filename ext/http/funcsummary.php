<?php

function hl($m)
{
    return sprintf("<blockquote>%s</blockquote>\n", highlight_string($m[1], true));
}
function mf($f, &$m)
{
    return preg_match_all(
        '/\/\* *\{\{\{ *proto (.*?)(\n|$)(.*?)PHP_(?:FUNCTION|METHOD)\((.*?)\)/s', 
        file_get_contents($f), $m);
}
function ff($t)
{
    $t = preg_replace('/^ \* /m', '', trim($t, "*/ \n"));
    $t = preg_replace_callback('/(\<\?php.*?\?\>)/s', 'hl', $t);
    $t = nl2br(preg_replace('/\n *\* */', "\n", $t));
    $t = preg_replace('/(\<br \/\>\n)+\<pre\>(\<br \/\>\n)+/', '</p><pre>', $t);
    $t = preg_replace('/(\<br \/\>\n)+\<\/pre\>(\<br \/\>\n)+/', '</pre><p>', $t);
    return sprintf('<p>%s</p>', ltrim($t, ' *'));
}

$preface = <<<_PREFACE
<html>
<head>
    <title>Function Summary of ext/%s</title>
    <style>
        body { 
            font-size: 80%%; 
            font-family: sans-serif; 
        } 
        h2 { 
            color: #339; 
            clear: both;
            font-size: 1.2em;
            background: #ffc;
            padding: .2em;
        } 
        p { 
            margin-left: 1em;
        } 
        pre { 
            font-size: 1.2em; 
        } 
        br { 
            display: none; 
        } 
        blockquote {
            margin-bottom: 3em;
            border: 1px solid #ccc;
            background: #f0f0f0;
            padding: 0em 1em;
            width: auto;
            float: left;
        }
        p br, pre code br { 
            display: block; 
        } 
    </style>
</head>
<body>
_PREFACE;

$footer = <<<_FOOTER
    <p><b>Generated at: %s</b></p>
</body>
</html>
_FOOTER;

if ($_SERVER['argc'] < 2) {
    die("Usage: {$_SERVER['argv'][0]} <file>[ <file> ...]\n");
}

printf($preface, basename(getcwd()));

foreach (array_slice($_SERVER['argv'], 1) as $f) {
    if (mf($f, $m)) {
        printf("<h1>%s</h1>\n", basename($f));
        foreach ($m[1] as $i => $p) {
            printf("<h2 id=\"%s\">%s</h2>\n%s\n", $m[4][$i], $p, ff($m[3][$i]));
        }
        print "<hr noshade>\n";
    }
}

printf($footer, date('r'));
?>
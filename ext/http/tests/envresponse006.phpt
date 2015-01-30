--TEST--
env response stream
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$f = tmpfile();

$r = new http\Env\Response;
$r->addHeader("foo", array("bar","baz"));
$r->getBody()->append("foo");

$r->send($f);

rewind($f);
var_dump(stream_get_contents($f));
?>
Done
--EXPECT--
Test
string(109) "HTTP/1.1 200 OK
Accept-Ranges: bytes
Foo: bar, baz
ETag: "0beec7b5ea3f0fdbc95d0dd47f3c5bc275da8a33"

foo"
Done

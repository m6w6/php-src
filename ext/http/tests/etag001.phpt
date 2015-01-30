--TEST--
etags with hash
--SKIPIF--
<?php 
include "skipif.inc"; 
version_compare(PHP_VERSION, "5.4.0", ">=") or die("skip PHP>=5.4 required");
?>
--FILE--
<?php
$body = new http\Message\Body;
$body->append("Hello, my old fellow.");
printf("%10s: %s\n", 
        "sha1", 
        $body->etag()
);
?>
DONE
--EXPECT--
      sha1: ad84012eabe27a61762a97138d9d2623f4f1a7a9
DONE

--TEST--
Test V8::setTimeLimit() : Time limit can be changed
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--FILE--
<?php

$JS = <<< EOT
var jsfunc = function() {
    PHP.incrTimeLimit();
    var start = (new Date()).getTime();

    var text = "abcdefghijklmnopqrstuvwyxz0123456789";
    while ((new Date()).getTime() - start < 150) {
        /* pass at least 150ms in the loop so the timer loop has plenty of
         * time to trigger. */
	var encoded = encodeURI(text);
    }
};
jsfunc;
EOT;

$v8 = new V8Js();
/* Set very short time limit, but enough so v8 can start up safely. */
$v8->setTimeLimit(20);

$v8->incrTimeLimit = function() use ($v8) {
    $v8->setTimeLimit(100);
};

$func = $v8->executeString($JS);
var_dump($func);

try {
    $func();
} catch (V8JsTimeLimitException $e) {
    print get_class($e); print PHP_EOL;
    print $e->getMessage(); print PHP_EOL;
}
?>
===EOF===
--EXPECTF--
object(V8Function)#%d (0) {
}
V8JsTimeLimitException
Script time limit of 100 milliseconds exceeded
===EOF===

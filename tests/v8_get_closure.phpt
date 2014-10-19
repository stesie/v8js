--TEST--
Test V8::executeString() : v8_get_closure on V8Object
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--FILE--
<?php

$v8 = new V8Js();
$a = $v8->executeString('var a = { bla: function() { print("Hallo\\n"); } }; a');
var_dump($a);

// v8_read_property
$b = $a->bla;

// invalidate $b
unset($v8);

try {
  // v8_get_closure (should throw), v8_call_method not reached.
  $b();
}
catch(Exception $e) {
  var_dump($e->getMessage());
}

?>
===EOF===
--EXPECTF--
object(V8Object)#%d (1) {
  ["bla"]=>
  object(V8Function)#%d (0) {
  }
}
string(55) "Can't access V8Object after V8Js instance is destroyed!"
===EOF===

--TEST--
Test V8Js::setModuleLoader : Global object available from module
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--FILE--
<?php

$v8 = new V8Js();
$v8->setModuleLoader(function($module) {
  return <<< EOT
  var_dump(typeof global);
  var_dump(global.var_dump === var_dump);
EOT;
});
$v8->executeString('require("foo");');
?>
===EOF===
--EXPECT--
string(6) "object"
bool(true)
===EOF===
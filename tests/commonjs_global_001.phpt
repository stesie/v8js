--TEST--
Test V8Js::setModuleLoader : Modules share global scope
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--FILE--
<?php

$JS = <<< EOT
global_var = 23;
var local_var = 42;

require("test");
var_dump(local_var);
EOT;

$v8 = new V8Js();
$v8->setModuleLoader(function($module) {
  return <<< EOT
  var_dump(typeof global_var);
  var_dump(typeof local_var);
  
  var local_var = 5;
EOT;
});

$v8->executeString($JS, 'module.js');
?>
===EOF===
--EXPECT--
string(6) "number"
string(9) "undefined"
int(42)
===EOF===

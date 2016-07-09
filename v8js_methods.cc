/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2013 The PHP Group                                |
  +----------------------------------------------------------------------+
  | http://www.opensource.org/licenses/mit-license.php  MIT License      |
  +----------------------------------------------------------------------+
  | Author: Jani Taskinen <jani.taskinen@iki.fi>                         |
  | Author: Patrick Reilly <preilly@php.net>                             |
  | Author: Stefan Siegl <stesie@brokenpipe.de>                          |
  +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php_v8js_macros.h"
#include "v8js_commonjs.h"

extern "C" {
#include "zend_exceptions.h"
}

/* forward declarations */
V8JS_METHOD(require);


/* global.exit - terminate execution */
V8JS_METHOD(exit) /* {{{ */
{
	v8::Isolate *isolate = info.GetIsolate();
	v8js_terminate_execution(isolate);
}
/* }}} */

/* global.sleep - sleep for passed seconds */
V8JS_METHOD(sleep) /* {{{ */
{
	php_sleep(info[0]->Int32Value());
}
/* }}} */

/* global.print - php print() */
V8JS_METHOD(print) /* {{{ */
{
	v8::Isolate *isolate = info.GetIsolate();
	int ret = 0;

	for (int i = 0; i < info.Length(); i++) {
		v8::String::Utf8Value str(info[i]);
		const char *cstr = ToCString(str);
		ret = PHPWRITE(cstr, strlen(cstr));
	}
	info.GetReturnValue().Set(V8JS_INT(ret));
}
/* }}} */

static void v8js_dumper(v8::Isolate *isolate, v8::Local<v8::Value> var, int level) /* {{{ */
{
	if (level > 1) {
		php_printf("%*c", (level - 1) * 2, ' ');
	}

	if (var.IsEmpty())
	{
		php_printf("<empty>\n");
		return;
	}
	if (var->IsNull() || var->IsUndefined() /* PHP compat */)
	{
		php_printf("NULL\n");
		return;
	}
	if (var->IsInt32())
	{
		php_printf("int(%ld)\n", (long) var->IntegerValue());
		return;
	}
	if (var->IsUint32())
	{
		php_printf("int(%lu)\n", (unsigned long) var->IntegerValue());
		return;
	}
	if (var->IsNumber())
	{
		php_printf("float(%f)\n", var->NumberValue());
		return;
	}
	if (var->IsBoolean())
	{
		php_printf("bool(%s)\n", var->BooleanValue() ? "true" : "false");
		return;
	}

	v8::TryCatch try_catch; /* object.toString() can throw an exception */
	v8::Local<v8::String> details;

	if(var->IsRegExp()) {
		v8::RegExp *re = v8::RegExp::Cast(*var);
		details = re->GetSource();
	}
	else {
		details = var->ToDetailString();

		if (try_catch.HasCaught()) {
			details = V8JS_SYM("<toString threw exception>");
		}
	}

	v8::String::Utf8Value str(details);
	const char *valstr = ToCString(str);
	size_t valstr_len = details->ToString()->Utf8Length();

	if (var->IsString())
	{
		php_printf("string(%zu) \"", valstr_len);
		PHPWRITE(valstr, valstr_len);
		php_printf("\"\n");
	}
	else if (var->IsDate())
	{
		// fake the fields of a PHP DateTime
		php_printf("Date(%s)\n", valstr);
	}
	else if (var->IsRegExp())
	{
		php_printf("regexp(/%s/)\n", valstr);
	}
	else if (var->IsArray())
	{
		v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(var);
		uint32_t length = array->Length();

		php_printf("array(%d) {\n", length);

		for (unsigned i = 0; i < length; i++) {
			php_printf("%*c[%d] =>\n", level * 2, ' ', i);
			v8js_dumper(isolate, array->Get(i), level + 1);
		}

		if (level > 1) {
			php_printf("%*c", (level - 1) * 2, ' ');
		}

		ZEND_PUTS("}\n");
	}
	else if (var->IsObject())
	{
		v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(var);
		V8JS_GET_CLASS_NAME(cname, object);
		int hash = object->GetIdentityHash();

		if (var->IsFunction() && strcmp(ToCString(cname), "Closure") != 0)
		{
			v8::String::Utf8Value csource(object->ToString());
			php_printf("object(Closure)#%d {\n%*c%s\n", hash, level * 2 + 2, ' ', ToCString(csource));
		}
		else
		{
			v8::Local<v8::Array> keys = object->GetOwnPropertyNames();
			uint32_t length = keys->Length();

			if (strcmp(ToCString(cname), "Array") == 0 ||
				strcmp(ToCString(cname), "V8Object") == 0) {
				php_printf("array");
			} else {
				php_printf("object(%s)#%d", ToCString(cname), hash);
			}
			php_printf(" (%d) {\n", length);

			for (unsigned i = 0; i < length; i++) {
				v8::Local<v8::String> key = keys->Get(i)->ToString();
				v8::String::Utf8Value kname(key);
				php_printf("%*c[\"%s\"] =>\n", level * 2, ' ', ToCString(kname));
				v8js_dumper(isolate, object->Get(key), level + 1);
			}
		}

		if (level > 1) {
			php_printf("%*c", (level - 1) * 2, ' ');
		}

		ZEND_PUTS("}\n");
	}
	else /* null, undefined, etc. */
	{
		php_printf("<%s>\n", valstr);
	}
}
/* }}} */

/* global.var_dump - Dump JS values */
V8JS_METHOD(var_dump) /* {{{ */
{
	v8::Isolate *isolate = info.GetIsolate();
	V8JS_TSRMLS_FETCH();

	for (int i = 0; i < info.Length(); i++) {
		v8js_dumper(isolate, info[i], 1);
	}

	info.GetReturnValue().Set(V8JS_NULL);
}

void v8js_forward_exception_or_terminate(v8js_ctx* c) {
	if (c->flags & V8JS_FLAG_PROPAGATE_PHP_EXCEPTIONS) {
		zval tmp_zv;
		ZVAL_OBJ(&tmp_zv, EG(exception));
		v8::Local<v8::Value> exception = zval_to_v8js(&tmp_zv, c->isolate);
		zend_clear_exception();
		throw exception;
	} else {
		v8js_terminate_execution(c->isolate);
		throw false;
	}
}

/* }}} */

static void v8js_call_custom_normaliser(v8js_ctx *c, const char *module_id,
										char **normalised_path /* out */, char **module_name /* out */)
{
	v8::Isolate *isolate = c->isolate;
	int call_result;
	zval params[2];
	zval normaliser_result;

	zend_try {
		{
			isolate->Exit();
			v8::Unlocker unlocker(isolate);

			ZVAL_STRING(&params[0], c->modules_base.back());
			ZVAL_STRING(&params[1], module_id);

			call_result = call_user_function_ex(EG(function_table), NULL, &c->module_normaliser,
												&normaliser_result, 2, params, 0, NULL);
		}

		isolate->Enter();

		if (call_result == FAILURE) {
			throw V8JS_SYM("Module normaliser callback failed");
		}
	}
	zend_catch {
		v8js_terminate_execution(isolate);
		V8JSG(fatal_error_abort) = 1;
		call_result = FAILURE;
	}
	zend_end_try();

	zval_dtor(&params[0]);
	zval_dtor(&params[1]);

	if(call_result == FAILURE) {
		throw V8JS_SYM("Module normaliser callback failed");
	}

	if (EG(exception)) {
		v8js_forward_exception_or_terminate(c);
	}

	if (Z_TYPE(normaliser_result) != IS_ARRAY) {
		zval_dtor(&normaliser_result);
		throw V8JS_SYM("Module normaliser didn't return an array");
	}

	HashTable *ht = HASH_OF(&normaliser_result);
	int num_elements = zend_hash_num_elements(ht);

	if(num_elements != 2) {
		zval_dtor(&normaliser_result);
		throw V8JS_SYM("Module normaliser expected to return array of 2 strings");
	}

	zval *data;
	ulong index = 0;

	ZEND_HASH_FOREACH_VAL(ht, data) {
		if (Z_TYPE_P(data) != IS_STRING) {
			convert_to_string(data);
		}

		switch(index++) {
		case 0: // normalised path
			*normalised_path = estrndup(Z_STRVAL_P(data), Z_STRLEN_P(data));
			break;

		case 1: // normalised module id
			*module_name = estrndup(Z_STRVAL_P(data), Z_STRLEN_P(data));
			break;
		}
	}
	ZEND_HASH_FOREACH_END();

	zval_dtor(&normaliser_result);
}

static void v8js_normalise_module_id(v8js_ctx *c, v8::Local<v8::Value> module_identifier,
									 char **normalised_module_id /* out */, char **normalised_path /* out */)
{
	v8::String::Utf8Value module_identifier_utf8(module_identifier);
	const char *module_id = ToCString(module_identifier_utf8);
	char *module_name;

	if (Z_TYPE(c->module_normaliser) == IS_NULL) {
		// No custom normalisation routine registered, use internal one
		v8js_commonjs_default_normaliser(c->modules_base.back(), module_id, normalised_path, &module_name);
	}
	else {
		v8js_call_custom_normaliser(c, module_id, normalised_path, &module_name);
	}

	*normalised_module_id = (char *)emalloc(strlen(*normalised_path) + 1 + strlen(module_name) + 1);
	**normalised_module_id = 0;

	if (strlen(*normalised_path) > 0) {
		strcat(*normalised_module_id, *normalised_path);
		strcat(*normalised_module_id, "/");
	}

	strcat(*normalised_module_id, module_name);
	efree(module_name);
}

static void v8js_call_module_loader(v8js_ctx *c, const char *normalised_module_id, zval *module_code /* out */)
{
	v8::Isolate *isolate = c->isolate;
	int call_result;
	zval params[1];

	{
		isolate->Exit();
		v8::Unlocker unlocker(isolate);

		zend_try {
			ZVAL_STRING(&params[0], normalised_module_id);
			call_result = call_user_function_ex(EG(function_table), NULL, &c->module_loader, module_code, 1, params, 0, NULL);
		}
		zend_catch {
			v8js_terminate_execution(isolate);
			V8JSG(fatal_error_abort) = 1;
		}
		zend_end_try();
	}

	isolate->Enter();
	zval_dtor(&params[0]);

	if (V8JSG(fatal_error_abort) || call_result == FAILURE) {
		throw static_cast<v8::Local<v8::Value>>(V8JS_SYM("Module loader callback failed"));
	}

	if (EG(exception)) {
		v8js_forward_exception_or_terminate(c);
	}
}

static v8::Local<v8::Object> v8js_call_module(v8js_ctx *c, const char *normalised_module_id, zval *module_code)
{
	v8::Isolate *isolate = c->isolate;

	// Convert the return value to string
	if (Z_TYPE(*module_code) != IS_STRING) {
		convert_to_string(module_code);
	}

	// Create a template for the global object and set the built-in global functions
	v8::Local<v8::ObjectTemplate> global_template = v8::ObjectTemplate::New();
	global_template->Set(V8JS_SYM("print"), v8::FunctionTemplate::New(isolate, V8JS_MN(print)), v8::ReadOnly);
	global_template->Set(V8JS_SYM("var_dump"), v8::FunctionTemplate::New(isolate, V8JS_MN(var_dump)), v8::ReadOnly);
	global_template->Set(V8JS_SYM("sleep"), v8::FunctionTemplate::New(isolate, V8JS_MN(sleep)), v8::ReadOnly);
	global_template->Set(V8JS_SYM("require"), v8::FunctionTemplate::New(isolate, V8JS_MN(require), v8::External::New(isolate, c)), v8::ReadOnly);

	// Add the exports object in which the module can return its API
	v8::Local<v8::ObjectTemplate> exports_template = v8::ObjectTemplate::New();
	global_template->Set(V8JS_SYM("exports"), exports_template);

	// Add the module object in which the module can have more fine-grained control over what it can return
	v8::Local<v8::ObjectTemplate> module_template = v8::ObjectTemplate::New();
	module_template->Set(V8JS_SYM("id"), V8JS_STR(normalised_module_id));
	global_template->Set(V8JS_SYM("module"), module_template);

	// Each module gets its own context so different modules do not affect each other
	v8::Local<v8::Context> context = v8::Local<v8::Context>::New(isolate, v8::Context::New(isolate, NULL, global_template));

	// Catch JS exceptions
	v8::TryCatch try_catch;

	v8::Locker locker(isolate);
	v8::Isolate::Scope isolate_scope(isolate);

	v8::EscapableHandleScope handle_scope(isolate);

	// Enter the module context
	v8::Context::Scope scope(context);
	// Set script identifier
	v8::Local<v8::String> sname = V8JS_STR(normalised_module_id);

	v8::Local<v8::String> source = V8JS_ZSTR(Z_STR(*module_code));

	// Create and compile script
	v8::Local<v8::Script> script = v8::Script::Compile(source, sname);

	// The script will be empty if there are compile errors
	if (script.IsEmpty()) {
		throw isolate->ThrowException(V8JS_SYM("Module script compile failed"));
	}

	// Run script
	script->Run();

	// Script possibly terminated, return immediately
	if (!try_catch.CanContinue()) {
		throw isolate->ThrowException(V8JS_SYM("Module script compile failed"));
	}

	// Handle runtime JS exceptions
	if (try_catch.HasCaught()) {
		// Rethrow the exception back to JS
		throw try_catch.ReThrow();
	}

	// Cache the module so it doesn't need to be compiled and run again
	// Ensure compatibility with CommonJS implementations such as NodeJS by playing nicely with module.exports and exports
	if (context->Global()->Has(V8JS_SYM("module"))
		&& context->Global()->Get(V8JS_SYM("module"))->IsObject()
		&& context->Global()->Get(V8JS_SYM("module"))->ToObject()->Has(V8JS_SYM("exports"))
		&& context->Global()->Get(V8JS_SYM("module"))->ToObject()->Get(V8JS_SYM("exports"))->IsObject()) {
		// If module.exports has been set then we cache this arbitrary value...
		return handle_scope.Escape(context->Global()->Get(V8JS_SYM("module"))->ToObject()->Get(V8JS_SYM("exports"))->ToObject());
	} else {
		// ...otherwise we cache the exports object itself
		return handle_scope.Escape(context->Global()->Get(V8JS_SYM("exports"))->ToObject());
	}
}

V8JS_METHOD(require)
{
	v8::Isolate *isolate = info.GetIsolate();

	// Get the extension context
	v8::Local<v8::External> data = v8::Local<v8::External>::Cast(info.Data());
	v8js_ctx *c = static_cast<v8js_ctx*>(data->Value());

	// Check that we have a module loader
	if(Z_TYPE(c->module_loader) == IS_NULL) {
		info.GetReturnValue().Set(isolate->ThrowException(V8JS_SYM("No module loader")));
		return;
	}

	char *normalised_module_id;
	char *normalised_path;

	try {
		v8js_normalise_module_id(c, info[0], &normalised_module_id, &normalised_path);
	}
	catch (v8::Local<v8::Value> error_message) {
		info.GetReturnValue().Set(isolate->ThrowException(error_message));
		return;
	}

	// Check for module cyclic dependencies
	for (std::vector<char *>::iterator it = c->modules_stack.begin(); it != c->modules_stack.end(); ++it)
    {
    	if (!strcmp(*it, normalised_module_id)) {
    		efree(normalised_module_id);
    		efree(normalised_path);

		info.GetReturnValue().Set(isolate->ThrowException(V8JS_SYM("Module cyclic dependency")));
		return;
    	}
    }

    // If we have already loaded and cached this module then use it
	if (c->modules_loaded.count(normalised_module_id) > 0) {
		v8::Persistent<v8::Object> newobj;
		newobj.Reset(isolate, c->modules_loaded[normalised_module_id]);
		info.GetReturnValue().Set(newobj);

		efree(normalised_module_id);
		efree(normalised_path);

		return;
	}

	// Callback to PHP to load the module code
	zval module_code;
	try {
		v8js_call_module_loader(c, normalised_module_id, &module_code);
	}
	catch (v8::Local<v8::Value> error_message) {
		efree(normalised_module_id);
		efree(normalised_path);

		if (!V8JSG(fatal_error_abort)) {
			info.GetReturnValue().Set(isolate->ThrowException(error_message));
		}
		return;
	}

	c->modules_stack.push_back(normalised_module_id);
	c->modules_base.push_back(normalised_path);

	try {
		v8::Local<v8::Object> newobj = v8js_call_module(c, normalised_module_id, &module_code);
		c->modules_loaded[normalised_module_id].Reset(isolate, newobj);
		info.GetReturnValue().Set(newobj);
	}
	catch (v8::Local<v8::Value> exception) {
		info.GetReturnValue().Set(exception);
		efree(normalised_module_id);
	}

	c->modules_stack.pop_back();
	c->modules_base.pop_back();

	zval_ptr_dtor(&module_code);
	efree(normalised_path);
}

void v8js_register_methods(v8::Local<v8::ObjectTemplate> global, v8js_ctx *c) /* {{{ */
{
	v8::Isolate *isolate = c->isolate;
	global->Set(V8JS_SYM("exit"), v8::FunctionTemplate::New(isolate, V8JS_MN(exit)), v8::ReadOnly);
	global->Set(V8JS_SYM("sleep"), v8::FunctionTemplate::New(isolate, V8JS_MN(sleep)), v8::ReadOnly);
	global->Set(V8JS_SYM("print"), v8::FunctionTemplate::New(isolate, V8JS_MN(print)), v8::ReadOnly);
	global->Set(V8JS_SYM("var_dump"), v8::FunctionTemplate::New(isolate, V8JS_MN(var_dump)), v8::ReadOnly);

	c->modules_base.push_back("");
	global->Set(V8JS_SYM("require"), v8::FunctionTemplate::New(isolate, V8JS_MN(require), v8::External::New(isolate, c)), v8::ReadOnly);
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

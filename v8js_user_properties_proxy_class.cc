/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2013 The PHP Group                                |
  +----------------------------------------------------------------------+
  | http://www.opensource.org/licenses/mit-license.php  MIT License      |
  +----------------------------------------------------------------------+
  | Author: Stefan Siegl <stesie@brokenpipe.de>                          |
  +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

extern "C" {
#include "php.h"
#include "ext/standard/php_string.h"
#include "ext/spl/spl_exceptions.h"
#include "zend_exceptions.h"
}

#include "php_v8js_macros.h"
#include "v8js_user_properties_proxy_class.h"

/* {{{ Class Entries */
zend_class_entry *php_ce_v8js_user_properties_proxy;
/* }}} */

PHP_METHOD(V8JsUserPropertiesProxy, __construct) /* {{{ */
{
	zval *v8js_obj;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O",
							  &v8js_obj, php_ce_v8js) == FAILURE) {
		RETURN_NULL();
	}

	zend_update_property(php_ce_v8js_user_properties_proxy, getThis(),
						 "v8js", 4, v8js_obj TSRMLS_CC);
}
/* }}} */

/* {{{ arginfo */
ZEND_BEGIN_ARG_INFO_EX(arginfo_v8js_user_properties_proxy_construct, 0, 0, 1)
	ZEND_ARG_INFO(0, v8js)
ZEND_END_ARG_INFO()
/* }}} */

static const zend_function_entry v8js_user_properties_proxy_methods[] = { /* {{{ */
	PHP_ME(V8JsUserPropertiesProxy,	__construct,	arginfo_v8js_user_properties_proxy_construct,	ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	{ NULL, NULL, NULL }
};
/* }}} */

PHP_MINIT_FUNCTION(v8js_user_properties_proxy_class) /* {{{ */
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "V8JsUserPropertiesProxy", v8js_user_properties_proxy_methods);
	php_ce_v8js_user_properties_proxy = zend_register_internal_class(&ce TSRMLS_CC);

	zend_declare_property_null(php_ce_v8js_user_properties_proxy, ZEND_STRL("v8js"), ZEND_ACC_PROTECTED TSRMLS_CC);

	return SUCCESS;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

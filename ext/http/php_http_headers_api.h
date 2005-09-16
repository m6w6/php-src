/*
   +----------------------------------------------------------------------+
   | PECL :: http                                                         |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license, that  |
   | is bundled with this package in the file LICENSE, and is available   |
   | through the world-wide-web at http://www.php.net/license/3_0.txt.    |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2004-2005 Michael Wallner <mike@php.net>               |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_HTTP_HEADERS_API_H
#define PHP_HTTP_HEADERS_API_H

#include "php_http_std_defs.h"
#include "php_http_info_api.h"

typedef enum {
	RANGE_OK,
	RANGE_NO,
	RANGE_ERR
} http_range_status;

#define http_parse_headers(h, a) _http_parse_headers_ex((h), Z_ARRVAL_P(a), 1, http_info_default_callback, NULL TSRMLS_CC)
#define http_parse_headers_ex(h, ht, p) _http_parse_headers_ex((h), (ht), (p), http_info_default_callback, NULL TSRMLS_CC)
#define http_parse_headers_cb(h, ht, p, f, d) _http_parse_headers_ex((h), (ht), (p), (f), (d) TSRMLS_CC)
PHP_HTTP_API STATUS _http_parse_headers_ex(const char *header, HashTable *headers, zend_bool prettify, http_info_callback callback_func, void **callback_data TSRMLS_DC);

#define http_get_request_headers(h) _http_get_request_headers_ex(Z_ARRVAL_P(h), 1 TSRMLS_CC)
#define http_get_request_headers_ex(h, p) _http_get_request_headers_ex((h), (p) TSRMLS_CC)
PHP_HTTP_API void _http_get_request_headers_ex(HashTable *headers, zend_bool prettify TSRMLS_DC);

typedef char *(*negotiate_func_t)(const char *test, double *quality, HashTable *supported TSRMLS_DC);

#define http_negotiate_language_func _http_negotiate_language_func
extern char *_http_negotiate_language_func(const char *test, double *quality, HashTable *supported TSRMLS_DC);
#define http_negotiate_charset_func _http_negotiate_charset_func
extern char *_http_negotiate_charset_func(const char *test, double *quality, HashTable *supported TSRMLS_DC);

#define http_negotiate_language(zsupported) http_negotiate_language_ex(Z_ARRVAL_P(zsupported))
#define http_negotiate_language_ex(supported) http_negotiate_q("HTTP_ACCEPT_LANGUAGE", (supported), http_negotiate_language_func)
#define http_negotiate_charset(zsupported) http_negotiate_charset_ex(Z_ARRVAL_P(zsupported))
#define http_negotiate_charset_ex(supported)  http_negotiate_q("HTTP_ACCEPT_CHARSET", (supported), http_negotiate_charset_func)
#define http_negotiate_q(e, s, n) _http_negotiate_q((e), (s), (n) TSRMLS_CC)
PHP_HTTP_API HashTable *_http_negotiate_q(const char *header, HashTable *supported, negotiate_func_t neg TSRMLS_DC);

#define http_get_request_ranges(r, l) _http_get_request_ranges((r), (l) TSRMLS_CC)
PHP_HTTP_API http_range_status _http_get_request_ranges(HashTable *ranges, size_t length TSRMLS_DC);

#define http_match_request_header(h, v) http_match_request_header_ex((h), (v), 0)
#define http_match_request_header_ex(h, v, c) _http_match_request_header_ex((h), (v), (c) TSRMLS_CC)
PHP_HTTP_API zend_bool _http_match_request_header_ex(const char *header, const char *value, zend_bool match_case TSRMLS_DC);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */


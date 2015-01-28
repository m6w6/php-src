/*
   +----------------------------------------------------------------------+
   | PHP Version 7                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2015 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Michael Wallner <mike@php.net>                              |
   +----------------------------------------------------------------------+
 */

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_raphf.h"

#ifndef PHP_RAPHF_DEBUG_PHANDLES
#	define PHP_RAPHF_DEBUG_PHANDLES 0
#endif
#if PHP_RAPHF_DEBUG_PHANDLES
#	undef inline
#	define inline
#endif

php_resource_factory_t *php_resource_factory_init(php_resource_factory_t *f,
		php_resource_factory_ops_t *fops, void *data, void (*dtor)(void *data))
{
	if (!f) {
		f = emalloc(sizeof(*f));
	}
	memset(f, 0, sizeof(*f));

	memcpy(&f->fops, fops, sizeof(*fops));

	f->data = data;
	f->dtor = dtor;

	f->refcount = 1;

	return f;
}

unsigned php_resource_factory_addref(php_resource_factory_t *rf)
{
	return ++rf->refcount;
}

void php_resource_factory_dtor(php_resource_factory_t *f)
{
	if (!--f->refcount) {
		if (f->dtor) {
			f->dtor(f->data);
		}
	}
}

void php_resource_factory_free(php_resource_factory_t **f)
{
	if (*f) {
		php_resource_factory_dtor(*f);
		if (!(*f)->refcount) {
			efree(*f);
			*f = NULL;
		}
	}
}

void *php_resource_factory_handle_ctor(php_resource_factory_t *f, void *init_arg)
{
	if (f->fops.ctor) {
		return f->fops.ctor(f->data, init_arg);
	}
	return NULL;
}

void *php_resource_factory_handle_copy(php_resource_factory_t *f, void *handle)
{
	if (f->fops.copy) {
		return f->fops.copy(f->data, handle);
	}
	return NULL;
}

void php_resource_factory_handle_dtor(php_resource_factory_t *f, void *handle)
{
	if (f->fops.dtor) {
		f->fops.dtor(f->data, handle);
	}
}

static inline php_persistent_handle_list_t *php_persistent_handle_list_init(
		php_persistent_handle_list_t *list)
{
	if (!list) {
		list = pemalloc(sizeof(*list), 1);
	}
	list->used = 0;
	zend_hash_init(&list->free, 0, NULL, NULL, 1);

	return list;
}

static int php_persistent_handle_apply_stat(zval *p, int argc, va_list argv,
		zend_hash_key *key)
{
	php_persistent_handle_list_t *list = Z_PTR_P(p);
	zval zsubentry, *zentry = va_arg(argv, zval *);

	array_init(&zsubentry);
	add_assoc_long_ex(&zsubentry, ZEND_STRL("used"), list->used);
	add_assoc_long_ex(&zsubentry, ZEND_STRL("free"),
			zend_hash_num_elements(&list->free));
	if (key->key) {
		add_assoc_zval_ex(zentry, key->key->val, key->key->len, &zsubentry);
	} else {
		add_index_zval(zentry, key->h, &zsubentry);
	}
	return ZEND_HASH_APPLY_KEEP;
}

static int php_persistent_handle_apply_statall(zval *p, int argc, va_list argv,
		zend_hash_key *key)
{
	php_persistent_handle_provider_t *provider = Z_PTR_P(p);
	HashTable *ht = va_arg(argv, HashTable *);
	zval zentry;

	array_init(&zentry);

	zend_hash_apply_with_arguments(&provider->list.free,
			php_persistent_handle_apply_stat, 1, &zentry);

	if (key->key) {
		zend_hash_update(ht, key->key, &zentry);
	} else {
		zend_hash_index_update(ht, key->h, &zentry);
	}

	return ZEND_HASH_APPLY_KEEP;
}

static int php_persistent_handle_apply_cleanup_ex(zval *p, void *arg)
{
	php_resource_factory_t *rf = arg;
	void *handle = Z_PTR_P(p);

#if PHP_RAPHF_DEBUG_PHANDLES
	fprintf(stderr, "DESTROY: %p\n", handle);
#endif
	php_resource_factory_handle_dtor(rf, handle);
	return ZEND_HASH_APPLY_REMOVE;
}

static int php_persistent_handle_apply_cleanup(zval *p, void *arg)
{
	php_resource_factory_t *rf = arg;
	php_persistent_handle_list_t *list = Z_PTR_P(p);

	zend_hash_apply_with_argument(&list->free,
			php_persistent_handle_apply_cleanup_ex, rf);
	if (list->used) {
		return ZEND_HASH_APPLY_KEEP;
	}
	zend_hash_destroy(&list->free);
#if PHP_RAPHF_DEBUG_PHANDLES
	fprintf(stderr, "LSTFREE: %p\n", list);
#endif
	pefree(list, 1);
	return ZEND_HASH_APPLY_REMOVE;
}

static inline void php_persistent_handle_list_dtor(
		php_persistent_handle_list_t *list,
		php_persistent_handle_provider_t *provider)
{
#if PHP_RAPHF_DEBUG_PHANDLES
	fprintf(stderr, "LSTDTOR: %p\n", list);
#endif
	zend_hash_apply_with_argument(&list->free,
			php_persistent_handle_apply_cleanup_ex, &provider->rf);
	zend_hash_destroy(&list->free);
}

static inline void php_persistent_handle_list_free(
		php_persistent_handle_list_t **list,
		php_persistent_handle_provider_t *provider)
{
	php_persistent_handle_list_dtor(*list, provider);
#if PHP_RAPHF_DEBUG_PHANDLES
	fprintf(stderr, "LSTFREE: %p\n", *list);
#endif
	pefree(*list, 1);
	*list = NULL;
}

static int php_persistent_handle_list_apply_dtor(zval *p, void *provider)
{
	php_persistent_handle_list_t *list = Z_PTR_P(p);

	php_persistent_handle_list_free(&list, provider );
	ZVAL_PTR(p, NULL);
	return ZEND_HASH_APPLY_REMOVE;
}

static inline php_persistent_handle_list_t *php_persistent_handle_list_find(
		php_persistent_handle_provider_t *provider, zend_string *ident)
{
	php_persistent_handle_list_t *list;
	zval *zlist = zend_symtable_find(&provider->list.free, ident);

	if (zlist && (list = Z_PTR_P(zlist))) {
#if PHP_RAPHF_DEBUG_PHANDLES
		fprintf(stderr, "LSTFIND: %p\n", list);
#endif
		return list;
	}

	if ((list = php_persistent_handle_list_init(NULL))) {
		zval p;

		ZVAL_PTR(&p, list);
		if (zend_symtable_update(&provider->list.free, ident, &p)) {
#if PHP_RAPHF_DEBUG_PHANDLES
			fprintf(stderr, "LSTFIND: %p (new)\n", list);
#endif
			return list;
		}
		php_persistent_handle_list_free(&list, provider);
	}

	return NULL;
}

static int php_persistent_handle_apply_cleanup_all(zval *p, int argc,
		va_list argv, zend_hash_key *key)
{
	php_persistent_handle_provider_t *provider = Z_PTR_P(p);
	zend_string *ident = va_arg(argv, zend_string *);
	php_persistent_handle_list_t *list;

	if (ident && ident->len) {
		if ((list = php_persistent_handle_list_find(provider, ident))) {
			zend_hash_apply_with_argument(&list->free,
					php_persistent_handle_apply_cleanup_ex,
					&provider->rf);
		}
	} else {
		zend_hash_apply_with_argument(&provider->list.free,
				php_persistent_handle_apply_cleanup, &provider->rf);
	}

	return ZEND_HASH_APPLY_KEEP;
}

static void php_persistent_handle_hash_dtor(zval *p)
{
	php_persistent_handle_provider_t *provider = Z_PTR_P(p);

	zend_hash_apply_with_argument(&provider->list.free,
			php_persistent_handle_list_apply_dtor, provider);
	zend_hash_destroy(&provider->list.free);
	php_resource_factory_dtor(&provider->rf);
	pefree(provider, 1);
}

ZEND_RESULT_CODE php_persistent_handle_provide(zend_string *name,
		php_resource_factory_ops_t *fops, void *data, void (*dtor)(void *))
{
	php_persistent_handle_provider_t *provider = pemalloc(sizeof(*provider), 1);

	if (php_persistent_handle_list_init(&provider->list)) {
		if (php_resource_factory_init(&provider->rf, fops, data, dtor)) {
			zval p, *rv;
			zend_string *ns;

#if PHP_RAPHF_DEBUG_PHANDLES
			fprintf(stderr, "PROVIDE: %p %s\n", PHP_RAPHF_G, name_str);
#endif

			ZVAL_PTR(&p, provider);
			if ((GC_FLAGS(name) & IS_STR_PERSISTENT)) {
				ns = name;
			} else {
				ns = zend_string_dup(name, 1);
			}
			rv = zend_symtable_update(&PG(persistent_handle_hash), ns, &p);
			if (ns != name) {
				zend_string_release(ns);
			}
			if (rv) {
				return SUCCESS;
			}
			php_resource_factory_dtor(&provider->rf);
		}
	}

	return FAILURE;
}


php_persistent_handle_factory_t *php_persistent_handle_concede(
		php_persistent_handle_factory_t *a,
		zend_string *name, zend_string *ident,
		php_persistent_handle_wakeup_t wakeup,
		php_persistent_handle_retire_t retire)
{
	zval *zprovider = zend_symtable_find(&PG(persistent_handle_hash), name);

	if (zprovider) {
		zend_bool free_a = 0;

		if ((free_a = !a)) {
			a = emalloc(sizeof(*a));
		}
		memset(a, 0, sizeof(*a));

		a->provider = Z_PTR_P(zprovider);
		a->ident = zend_string_copy(ident);
		a->wakeup = wakeup;
		a->retire = retire;
		a->free_on_abandon = free_a;
	} else {
		a = NULL;
	}

#if PHP_RAPHF_DEBUG_PHANDLES
	fprintf(stderr, "CONCEDE: %p %p (%s) (%s)\n", PHP_RAPHF_G,
			a ? a->provider : NULL, name->val, ident->val);
#endif

	return a;
}

void php_persistent_handle_abandon(php_persistent_handle_factory_t *a)
{
	zend_bool f = a->free_on_abandon;

#if PHP_RAPHF_DEBUG_PHANDLES
	fprintf(stderr, "ABANDON: %p\n", a->provider);
#endif

	zend_string_release(a->ident);
	memset(a, 0, sizeof(*a));
	if (f) {
		efree(a);
	}
}

void *php_persistent_handle_acquire(php_persistent_handle_factory_t *a, void *init_arg)
{
	int key;
	zval *p;
	zend_ulong index;
	void *handle = NULL;
	php_persistent_handle_list_t *list;

	list = php_persistent_handle_list_find(a->provider, a->ident);
	if (list) {
		zend_hash_internal_pointer_end(&list->free);
		key = zend_hash_get_current_key(&list->free, NULL, &index);
		p = zend_hash_get_current_data(&list->free);
		if (p && HASH_KEY_NON_EXISTENT != key) {
			handle = Z_PTR_P(p);
			if (a->wakeup) {
				a->wakeup(a, &handle);
			}
			zend_hash_index_del(&list->free, index);
		} else {
			handle = php_resource_factory_handle_ctor(&a->provider->rf, init_arg);
		}
#if PHP_RAPHF_DEBUG_PHANDLES
		fprintf(stderr, "CREATED: %p\n", handle);
#endif
		if (handle) {
			++a->provider->list.used;
			++list->used;
		}
	}

	return handle;
}

void *php_persistent_handle_accrete(php_persistent_handle_factory_t *a, void *handle)
{
	void *new_handle = NULL;
	php_persistent_handle_list_t *list;

	new_handle = php_resource_factory_handle_copy(&a->provider->rf, handle);
	if (handle) {
		list = php_persistent_handle_list_find(a->provider, a->ident);
		if (list) {
			++list->used;
		}
		++a->provider->list.used;
	}

	return new_handle;
}

void php_persistent_handle_release(php_persistent_handle_factory_t *a, void *handle)
{
	php_persistent_handle_list_t *list;

	list = php_persistent_handle_list_find(a->provider, a->ident);
	if (list) {
		if (a->provider->list.used >= PG(persistent_handle_limit)) {
#if PHP_RAPHF_DEBUG_PHANDLES
			fprintf(stderr, "DESTROY: %p\n", handle);
#endif
			php_resource_factory_handle_dtor(&a->provider->rf, handle);
		} else {
			if (a->retire) {
				a->retire(a, &handle);
			}
			zend_hash_next_index_insert_ptr(&list->free, handle);
		}

		--a->provider->list.used;
		--list->used;
	}
}

void php_persistent_handle_cleanup(zend_string *name, zend_string *ident)
{
	php_persistent_handle_provider_t *provider;
	php_persistent_handle_list_t *list;

	if (name) {
		zval *zprovider = zend_symtable_find(&PG(persistent_handle_hash),
				name);

		if (zprovider && (provider = Z_PTR_P(zprovider))) {
			if (ident) {
				list = php_persistent_handle_list_find(provider, ident);
				if (list) {
					zend_hash_apply_with_argument(&list->free,
							php_persistent_handle_apply_cleanup_ex,
							&provider->rf);
				}
			} else {
				zend_hash_apply_with_argument(&provider->list.free,
						php_persistent_handle_apply_cleanup,
						&provider->rf);
			}
		}
	} else {
		zend_hash_apply_with_arguments(
				&PG(persistent_handle_hash),
				php_persistent_handle_apply_cleanup_all, 1, ident);
	}
}

HashTable *php_persistent_handle_statall(HashTable *ht)
{
	if (zend_hash_num_elements(&PG(persistent_handle_hash))) {
		if (!ht) {
			ALLOC_HASHTABLE(ht);
			zend_hash_init(ht, 0, NULL, ZVAL_PTR_DTOR, 0);
		}
		zend_hash_apply_with_arguments(
				&PG(persistent_handle_hash),
				php_persistent_handle_apply_statall, 1, ht);
	} else if (ht) {
		ht = NULL;
	}

	return ht;
}

static php_resource_factory_ops_t php_persistent_handle_resource_factory_ops = {
	(php_resource_factory_handle_ctor_t) php_persistent_handle_acquire,
	(php_resource_factory_handle_copy_t) php_persistent_handle_accrete,
	(php_resource_factory_handle_dtor_t) php_persistent_handle_release
};

php_resource_factory_ops_t *php_persistent_handle_get_resource_factory_ops(void)
{
	return &php_persistent_handle_resource_factory_ops;
}

static HashTable *php_persistent_handles_global_hash;

void php_persistent_handles_startup(void)
{
	PG(persistent_handle_limit) = -1;

	zend_hash_init(&PG(persistent_handle_hash), 0, NULL,
			php_persistent_handle_hash_dtor, 1);
	if (php_persistent_handles_global_hash) {
		zend_hash_copy(&PG(persistent_handle_hash),
				php_persistent_handles_global_hash, NULL);
	} else {
		php_persistent_handles_global_hash = &PG(persistent_handle_hash);
	}
}

void php_persistent_handles_shutdown(void)
{
	zend_hash_destroy(&PG(persistent_handle_hash));
	php_persistent_handles_global_hash = NULL;
}

static int php_persistent_handle_apply_info_ex(zval *p, int argc,
		va_list argv, zend_hash_key *key)
{
	php_persistent_handle_list_t *list = Z_PTR_P(p);
	zend_hash_key *super_key = va_arg(argv, zend_hash_key *);
	char used[21], free[21];

	slprintf(used, sizeof(used), "%u", list->used);
	slprintf(free, sizeof(free), "%d", zend_hash_num_elements(&list->free));

	php_info_print_table_row(4, super_key->key->val, key->key->val, used, free);

	return ZEND_HASH_APPLY_KEEP;
}

static int php_persistent_handle_apply_info(zval *p, int argc,
		va_list argv, zend_hash_key *key)
{
	php_persistent_handle_provider_t *provider = Z_PTR_P(p);

	zend_hash_apply_with_arguments(&provider->list.free,
			php_persistent_handle_apply_info_ex, 1, key);

	return ZEND_HASH_APPLY_KEEP;
}

void php_persistent_handles_info_print(void)
{
	php_info_print_table_start();
	php_info_print_table_colspan_header(4, "Persistent handles");
	php_info_print_table_header(4, "Provider", "Ident", "Used", "Free");
	zend_hash_apply_with_arguments(
			&PG(persistent_handle_hash),
			php_persistent_handle_apply_info, 0);
	php_info_print_table_end();
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

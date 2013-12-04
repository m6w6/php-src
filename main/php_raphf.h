/*
    +--------------------------------------------------------------------+
    | PECL :: raphf                                                      |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2013, Michael Wallner <mike@php.net>                 |
    +--------------------------------------------------------------------+
*/

#ifndef PHP_RAPHF_H
#define PHP_RAPHF_H

#ifndef DOXYGEN

extern zend_module_entry raphf_module_entry;
#define phpext_raphf_ptr &raphf_module_entry

#define PHP_RAPHF_VERSION "1.0.4"

#ifdef PHP_WIN32
#	define PHP_RAPHF_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_RAPHF_API extern __attribute__ ((visibility("default")))
#else
#	define PHP_RAPHF_API extern
#endif

#ifdef ZTS
#	include "TSRM.h"
#endif

#endif

/**
 * A resource constructor.
 *
 * @param opaque is the \a data from php_persistent_handle_provide()
 * @param init_arg is the \a init_arg from php_resource_factory_init()
 * @return the created (persistent) handle
 */
typedef void *(*php_resource_factory_handle_ctor_t)(void *opaque,
		void *init_arg TSRMLS_DC);

/**
 * The copy constructor of a resource.
 *
 * @param opaque the factory's data
 * @param handle the (persistent) handle to copy
 */
typedef void *(*php_resource_factory_handle_copy_t)(void *opaque,
		void *handle TSRMLS_DC);

/**
 * The destructor of a resource.
 *
 * @param opaque the factory's data
 * @param handle the handle to destroy
 */
typedef void (*php_resource_factory_handle_dtor_t)(void *opaque,
		void *handle TSRMLS_DC);

/**
 * The resource ops consisting of a ctor, a copy ctor and a dtor.
 *
 * Define this ops and register them with php_persistent_handle_provide()
 * in MINIT.
 */
typedef struct php_resource_factory_ops {
	/** The resource constructor */
	php_resource_factory_handle_ctor_t ctor;
	/** The resource's copy constructor */
	php_resource_factory_handle_copy_t copy;
	/** The resource's destructor */
	php_resource_factory_handle_dtor_t dtor;
} php_resource_factory_ops_t;

/**
 * The resource factory.
 */
typedef struct php_resource_factory {
	/** The resource ops */
	php_resource_factory_ops_t fops;
	/** Opaque user data */
	void *data;
	/** User data destructor */
	void (*dtor)(void *data);
	/** How often this factory is referenced */
	unsigned refcount;
} php_resource_factory_t;

/**
 * Initialize a resource factory.
 *
 * If you register a \a dtor for a resource factory used with a persistent
 * handle provider, be sure to call php_persistent_handle_cleanup() for your
 * registered provider in MSHUTDOWN, else the dtor will point to no longer
 * available memory if the extension has already been unloaded.
 *
 * @param f the factory to initialize; if NULL allocated on the heap
 * @param fops the resource ops to assign to the factory
 * @param data opaque user data; may be NULL
 * @param dtor a destructor for the data; may be NULL
 * @return \a f or an allocated resource factory
 */
PHP_RAPHF_API php_resource_factory_t *php_resource_factory_init(
		php_resource_factory_t *f, php_resource_factory_ops_t *fops, void *data,
		void (*dtor)(void *data));

/**
 * Increase the refcount of the resource factory.
 *
 * @param rf the resource factory
 * @return the new refcount
 */
PHP_RAPHF_API unsigned php_resource_factory_addref(php_resource_factory_t *rf);

/**
 * Destroy the resource factory.
 *
 * If the factory's refcount reaches 0, the \a dtor for \a data is called.
 *
 * @param f the resource factory
 */
PHP_RAPHF_API void php_resource_factory_dtor(php_resource_factory_t *f);

/**
 * Destroy and free the resource factory.
 *
 * Calls php_resource_factory_dtor() and frees \æ f if the factory's refcount
 * reached 0.
 *
 * @param f the resource factory
 */
PHP_RAPHF_API void php_resource_factory_free(php_resource_factory_t **f);

/**
 * Construct a resource by the resource factory \a f
 *
 * @param f the resource factory
 * @param init_arg for the resource constructor
 * @return the new resource
 */
PHP_RAPHF_API void *php_resource_factory_handle_ctor(php_resource_factory_t *f,
		void *init_arg TSRMLS_DC);

/**
 * Create a copy of the resource \a handle
 *
 * @param f the resource factory
 * @param handle the resource to copy
 * @return the copy
 */
PHP_RAPHF_API void *php_resource_factory_handle_copy(php_resource_factory_t *f,
		void *handle TSRMLS_DC);

/**
 * Destroy (and free) the resource
 *
 * @param f the resource factory
 * @param handle the resource to destroy
 */
PHP_RAPHF_API void php_resource_factory_handle_dtor(php_resource_factory_t *f,
		void *handle TSRMLS_DC);

/**
 * Persistent handles storage
 */
typedef struct php_persistent_handle_list {
	/** Storage of free resources */
	HashTable free;
	/** Count of acquired resources */
	ulong used;
} php_persistent_handle_list_t;

/**
 * Definition of a persistent handle provider.
 * Holds a resource factory an a persistent handle list.
 */
typedef struct php_persistent_handle_provider {
	 /**
	  * The list of free handles.
	  * Hash of "ident" => array(handles) entries. Persistent handles are
	  * acquired out of this list.
	  */
	php_persistent_handle_list_t list;

	/**
	 * The resource factory.
	 * New handles are created by this factory.
	 */
	php_resource_factory_t rf;
} php_persistent_handle_provider_t;

typedef struct php_persistent_handle_factory php_persistent_handle_factory_t;

/**
 * Wakeup the persistent handle on re-acquisition.
 */
typedef void (*php_persistent_handle_wakeup_t)(
		php_persistent_handle_factory_t *f, void **handle TSRMLS_DC);
/**
 * Retire the persistent handle on release.
 */
typedef void (*php_persistent_handle_retire_t)(
		php_persistent_handle_factory_t *f, void **handle TSRMLS_DC);

/**
 * Definition of a persistent handle factory.
 *
 * php_persistent_handle_concede() will return a pointer to a
 * php_persistent_handle_factory if a provider for the \a name_str has
 * been registered with php_persistent_handle_provide().
 */
struct php_persistent_handle_factory {
	/** The persistent handle provider */
	php_persistent_handle_provider_t *provider;
	/** The persistent handle wakeup routine; may be NULL */
	php_persistent_handle_wakeup_t wakeup;
	/** The persistent handle retire routine; may be NULL */
	php_persistent_handle_retire_t retire;

	/** The ident for which this factory manages resources */
	struct {
		/** ident string */
		char *str;
		/** ident length */
		size_t len;
	} ident;

	/** Whether it has to be free'd on php_persistent_handle_abandon() */
	unsigned free_on_abandon:1;
};

/**
 * Register a persistent handle provider in MINIT.
 *
 * Registers a factory provider for \a name_str with \a fops resource factory
 * ops. Call this in your MINIT.
 *
 * A php_resource_factory will be created with \a fops, \a data and \a dtor
 * and will be stored together with a php_persistent_handle_list in the global
 * raphf hash.
 *
 * A php_persistent_handle_factory can then be retrieved by
 * php_persistent_handle_concede() at runtime.
 *
 * @param name_str the provider name, e.g. "http\Client\Curl"
 * @param name_len the provider name length, e.g. strlen("http\Client\Curl")
 * @param fops the resource factory ops
 * @param data opaque user data
 * @param dtor \a data destructor
 * @return SUCCESS/FAILURE
 */
PHP_RAPHF_API int /* SUCCESS|FAILURE */ php_persistent_handle_provide(
		const char *name_str, size_t name_len, php_resource_factory_ops_t *fops,
		void *data, void (*dtor)(void *) TSRMLS_DC);

/**
 * Retrieve a persistent handle factory at runtime.
 *
 * If a persistent handle provider has been registered for \a name_str, a new
 * php_persistent_handle_factory creating resources in the \a ident_str
 * namespace will be constructed.
 *
 * The wakeup routine \a wakeup and the retire routine \a retire will be
 * assigned to the new php_persistent_handle_factory.
 *
 * @param a pointer to a factory; allocated on the heap if NULL
 * @param name_str the provider name, e.g. "http\Client\Curl"
 * @param name_len the provider name length, e.g. strlen("http\Client\Curl")
 * @param ident_str the subsidiary namespace, e.g. "php.net:80"
 * @param ident_len the subsidiary namespace lenght, e.g. strlen("php.net:80")
 * @param wakeup any persistent handle wakeup routine
 * @param retire any persistent handle retire routine
 * @return \a a or an allocated persistent handle factory
 */
PHP_RAPHF_API php_persistent_handle_factory_t *php_persistent_handle_concede(
		php_persistent_handle_factory_t *a, const char *name_str,
		size_t name_len, const char *ident_str, size_t ident_len,
		php_persistent_handle_wakeup_t wakeup,
		php_persistent_handle_retire_t retire TSRMLS_DC);

/**
 * Abandon the persistent handle factory.
 *
 * Destroy a php_persistent_handle_factory created by
 * php_persistent_handle_concede(). If the memory for the factory was allocated,
 * it will automatically be free'd.
 *
 * @param a the persistent handle factory to destroy
 */
PHP_RAPHF_API void php_persistent_handle_abandon(
		php_persistent_handle_factory_t *a);

/**
 * Acquire a persistent handle.
 *
 * That is, either re-use a resource from the free list or create a new handle.
 *
 * If a handle is acquired from the free list, the
 * php_persistent_handle_factory::wakeup callback will be executed for that
 * handle.
 *
 * @param a the persistent handle factory
 * @param init_arg the \a init_arg for php_resource_factory_handle_ctor()
 * @return the acquired resource
 */
PHP_RAPHF_API void *php_persistent_handle_acquire(
		php_persistent_handle_factory_t *a, void *init_arg TSRMLS_DC);

/**
 * Release a persistent handle.
 *
 * That is, either put it back into the free list for later re-use or clean it
 * up with php_resource_factory_handle_dtor().
 *
 * If a handle is put back into the free list, the
 * php_persistent_handle_factory::retire callback will be executed for that
 * handle.
 *
 * @param a the persistent handle factory
 * @param handle the handle to release
 */
PHP_RAPHF_API void php_persistent_handle_release(
		php_persistent_handle_factory_t *a, void *handle TSRMLS_DC);

/**
 * Copy a persistent handle.
 *
 * Let the underlying resource factory copy the \a handle.
 *
 * @param a the persistent handle factory
 * @param handle the resource to accrete
 */
PHP_RAPHF_API void *php_persistent_handle_accrete(
		php_persistent_handle_factory_t *a, void *handle TSRMLS_DC);

/**
 * Retrieve persistent handle resource factory ops.
 *
 * These ops can be used to mask a persistent handle factory as
 * resource factory itself, so you can transparently use the
 * resource factory API, both for persistent and non-persistent
 * ressources.
 *
 * Example:
 * ~~~~~~~~~~~~~~~{.c}
 * php_resource_factory_t *create_my_rf(const char *persistent_id_str,
 *                                      size_t persistent_id_len TSRMLS_DC)
 * {
 *     php_resource_factory_t *rf;
 *
 *     if (persistent_id_str) {
 *         php_persistent_handle_factory_t *pf;
 *         php_resource_factory_ops_t *ops;
 *
 *         ops = php_persistent_handle_get_resource_factory_ops();
 *
 *         pf = php_persistent_handle_concede(NULL, "my", 2,
 *             persistent_id_str, persistent_id_len, NULL, NULL TSRMLS_CC);
 *
 *         rf = php_resource_factory_init(NULL, ops, pf, php_persistent_handle_abandon);
 *     } else {
 *         rf = php_resource_factory_init(NULL, &myops, NULL, NULL);
 *     }
 *     return rf;
 * }
 * ~~~~~~~~~~~~~~~
 */
PHP_RAPHF_API php_resource_factory_ops_t *
php_persistent_handle_get_resource_factory_ops(void);

/**
 * Clean persistent handles up.
 *
 * Destroy persistent handles of provider \a name_str and in subsidiary
 * namespace \a ident_str.
 *
 * If \a name_str is NULL, all persistent handles of all providers with a
 * matching \a ident_str will be cleaned up.
 *
 * If \a ident_str is NULL all persistent handles of the provider will be
 * cleaned up.
 *
 * Ergo, if both, \a name_str and \a ident_str are NULL, then all
 * persistent handles will be cleaned up.
 *
 * You must call this in MSHUTDOWN, if your resource factory ops hold a
 * registered php_resource_factory::dtor, else the dtor will point to
 * memory not any more available if the extension has already been unloaded.
 *
 * @param name_str the provider name; may be NULL
 * @param name_len the provider name length
 * @param ident_str the subsidiary namespace name; may be NULL
 * @param ident_len the subsidiary namespace name length
 */
PHP_RAPHF_API void php_persistent_handle_cleanup(const char *name_str,
		size_t name_len, const char *ident_str, size_t ident_len TSRMLS_DC);

/**
 * Retrieve statistics about the current process/thread's persistent handles.
 *
 * @return a HashTable like:
 * ~~~~~~~~~~~~~~~
 *     [
 *         "name" => [
 *             "ident" => [
 *                 "used" => 1,
 *                 "free" => 0,
 *             ]
 *         ]
 *     ]
 * ~~~~~~~~~~~~~~~
 */
PHP_RAPHF_API HashTable *php_persistent_handle_statall(HashTable *ht TSRMLS_DC);

#endif	/* PHP_RAPHF_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

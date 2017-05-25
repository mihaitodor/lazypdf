#include "faster_raster.h"

// Format with:
// indent -linux -br -brf

// Have to wrap this macro so we can call from Cgo
fz_context *cgo_fz_new_context(const fz_alloc_context * alloc,
			       const fz_locks_context * locks,
			       size_t max_store) {
	return fz_new_context(alloc, locks, max_store);
}

// Cast a ptrdiff_t to an int for use in Cgo. Go types won't let
// us do it in Go.
int cgo_ptr_cast(ptrdiff_t ptr) {
	return (int)(ptr);
}

// Wrap fz_open_document, which uses a try/catch exception handler
// that we can't easily use from Go.
fz_document *cgo_open_document(fz_context *ctx, const char *filename) {
    fz_document *doc = NULL;

    fz_try(ctx) {
		doc = fz_open_document(ctx, filename);
	}
	fz_catch(ctx) {
        fprintf(stderr, "cannot open document: %s\n", fz_caught_message(ctx));
		return NULL;
	}

	return doc;
}

// Wrap fz_drop_document to handle the exception trap when something is
// wrong. We can't easily do this from Go.
void cgo_drop_document(fz_context *ctx, fz_document *doc) {
	fz_try(ctx) {
		fz_drop_document(ctx, doc);
	}
	fz_catch(ctx) {
        fprintf(stderr, "cannot drop document: %s\n", fz_caught_message(ctx));
	}
}

// Calls back into the Go code to lock a mutex
void lock_mutex(void *locks, int lock_no) {
	pthread_mutex_t *m = (pthread_mutex_t *) locks;
	int result;

	if ((result = pthread_mutex_lock(&m[lock_no])) != 0) {
		fprintf(stderr, "lock_mutex failed! %s\n", strerror(result));
	}
}

// Calls back into the Go code to lock a mutex
void unlock_mutex(void *locks, int lock_no) {
	pthread_mutex_t *m = (pthread_mutex_t *) locks;
	int result;

	if ((result = pthread_mutex_unlock(&m[lock_no])) != 0) {
		fprintf(stderr, "unlock_mutex failed! %s\n", strerror(result));
	}
}

// Initializes the lock structure in C since we can't manage
// the memory properly from Go.
fz_locks_context *new_locks() {
	fz_locks_context *locks = malloc(sizeof(fz_locks_context));

	if (locks == NULL) {
		fprintf(stderr, "Unable to allocate locks!\n");
		return NULL;
	}

	pthread_mutex_t *mutexes =
	    malloc(sizeof(pthread_mutex_t) * FZ_LOCK_MAX);

	if (mutexes == NULL) {
		fprintf(stderr, "Unable to allocate mutexes!\n");
		return NULL;
	}

	int i, result;
	for (i = 0; i < FZ_LOCK_MAX; i++) {
		result = pthread_mutex_init(&mutexes[i], NULL);
		if (result != 0) {
			fprintf(stderr, "Failed to initialize mutex: %s\n", strerror(result));
		}
	}

	// Pass in the initialized mutexes and the two C funcs
	// that will handle the pthread mutexes.
	locks->lock = &lock_mutex;
	locks->unlock = &unlock_mutex;
	locks->user = mutexes;

	return locks;
}

// Free the lock structure in C since we allocated the memory
// here.
void free_locks(fz_locks_context * locks) {
	free(locks->user);
	free(locks);
}

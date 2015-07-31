#ifndef LOCKFILE_H
#define LOCKFILE_H

/*
 * File write-locks as used by Git.
 *
 * The lockfile API serves two purposes:
 *
 * * Mutual exclusion and atomic file updates. When we want to change
 *   a file, we create a lockfile `<filename>.lock`, write the new
 *   file contents into it, and then rename the lockfile to its final
 *   destination `<filename>`. We create the `<filename>.lock` file
 *   with `O_CREAT|O_EXCL` so that we can notice and fail if somebody
 *   else has already locked the file, then atomically rename the
 *   lockfile to its final destination to commit the changes and
 *   unlock the file.
 *
 * * Automatic cruft removal. If the program exits after we lock a
 *   file but before the changes have been committed, we want to make
 *   sure that we remove the lockfile. This is done by remembering the
 *   lockfiles we have created in a linked list and setting up an
 *   `atexit(3)` handler and a signal handler that clean up the
 *   lockfiles. This mechanism ensures that outstanding lockfiles are
 *   cleaned up if the program exits (including when `die()` is
 *   called) or if the program is terminated by a signal.
 *
 * Please note that lockfiles only block other writers. Readers do not
 * block, but they are guaranteed to see either the old contents of
 * the file or the new contents of the file (assuming that the
 * filesystem implements `rename(2)` atomically).
 *
 * Most of the heavy lifting is done by the tempfile module (see
 * "tempfile.h").
 *
 * Calling sequence
 * ----------------
 *
 * The caller:
 *
 * * Allocates a `struct lock_file` either as a static variable or on
 *   the heap, initialized to zeros. Once you use the structure to
 *   call the `hold_lock_file_for_*()` family of functions, it belongs
 *   to the lockfile subsystem and its storage must remain valid
 *   throughout the life of the program (i.e. you cannot use an
 *   on-stack variable to hold this structure).
 *
 * * Attempts to create a lockfile by calling
 *   `hold_lock_file_for_update()` or `hold_lock_file_for_append()`.
 *
 * * Writes new content for the destination file by either:
 *
 *   * writing to the file descriptor returned by the
 *     `hold_lock_file_for_*()` functions (also available via
 *     `lock->fd`).
 *
 *   * calling `fdopen_tempfile(lk->tempfile)` to get a `FILE` pointer
 *     for the open file and writing to the file using stdio.
 *
 * When finished writing, the caller can:
 *
 * * Close the file descriptor and rename the lockfile to its final
 *   destination by calling `commit_lock_file()` or
 *   `commit_lock_file_to()`.
 *
 * * Close the file descriptor and remove the lockfile by calling
 *   `rollback_lock_file()`.
 *
 * It is also permissable to call the following functions on the
 * underlying tempfile object:
 *
 * * close_tempfile(lk->tempfile)
 *
 * * reopen_tempfile(lk->tempfile)
 *
 * * fdopen_tempfile(lk->tempfile, mode)
 *
 * See "tempfile.h" for more information.
 *
 * Even after the lockfile is committed or rolled back, the
 * `lock_file` object must not be freed or altered by the caller.
 * However, it may be reused; just pass it to another call of
 * `hold_lock_file_for_update()` or `hold_lock_file_for_append()`.
 *
 * If the program exits before `commit_lock_file()`,
 * `commit_lock_file_to()`, or `rollback_lock_file()` is called, the
 * tempfile module will close and remove the lockfile, thereby rolling
 * back any uncommitted changes.
 *
 * Under the covers, a lockfile is just a tempfile with a few helper
 * functions. In particular, the state diagram and the cleanup
 * machinery are all implemented in the tempfile module.
 *
 *
 * Error handling
 * --------------
 *
 * The `hold_lock_file_for_*()` functions return a file descriptor on
 * success or -1 on failure (unless `LOCK_DIE_ON_ERROR` is used; see
 * "flags" below). On errors, `errno` describes the reason for
 * failure. Errors can be reported by passing `errno` to
 * `unable_to_lock_message()` or `unable_to_lock_die()`.
 *
 * Similarly, `commit_lock_file` and `commit_lock_file_to` return 0 on
 * success. On failure they set `errno` appropriately, do their best
 * to roll back the lockfile, and return -1.
 */

struct lock_file {
	struct tempfile tempfile;
};

/* String appended to a filename to derive the lockfile name: */
#define LOCK_SUFFIX ".lock"
#define LOCK_SUFFIX_LEN 5


/*
 * Flags
 * -----
 *
 * The following flags can be passed to `hold_lock_file_for_update()`
 * or `hold_lock_file_for_append()`.
 */

/*
 * If a lock is already taken for the file, `die()` with an error
 * message. If this flag is not specified, trying to lock a file that
 * is already locked returns -1 to the caller.
 */
#define LOCK_DIE_ON_ERROR 1

/*
 * Usually symbolic links in the destination path are resolved. This
 * means that (1) the lockfile is created by adding ".lock" to the
 * resolved path, and (2) upon commit, the resolved path is
 * overwritten. However, if `LOCK_NO_DEREF` is set, then the lockfile
 * is created by adding ".lock" to the path argument itself. This
 * option is used, for example, when detaching a symbolic reference,
 * which for backwards-compatibility reasons, can be a symbolic link
 * containing the name of the referred-to-reference.
 */
#define LOCK_NO_DEREF 2

/*
 * Attempt to create a lockfile for the file at `path` and return a
 * file descriptor for writing to it, or -1 on error. If the file is
 * currently locked, retry with quadratic backoff for at least
 * timeout_ms milliseconds. If timeout_ms is 0, try exactly once; if
 * timeout_ms is -1, retry indefinitely. The flags argument and error
 * handling are described above.
 */
extern int hold_lock_file_for_update_timeout(
		struct lock_file *lk, const char *path,
		int flags, long timeout_ms);

/*
 * Attempt to create a lockfile for the file at `path` and return a
 * file descriptor for writing to it, or -1 on error. The flags
 * argument and error handling are described above.
 */
static inline int hold_lock_file_for_update(
		struct lock_file *lk, const char *path,
		int flags)
{
	return hold_lock_file_for_update_timeout(lk, path, flags, 0);
}

/*
 * Like `hold_lock_file_for_update()`, but before returning copy the
 * existing contents of the file (if any) to the lockfile and position
 * its write pointer at the end of the file. The flags argument and
 * error handling are described above.
 */
extern int hold_lock_file_for_append(struct lock_file *lk,
				     const char *path, int flags);

/*
 * Append an appropriate error message to `buf` following the failure
 * of `hold_lock_file_for_update()` or `hold_lock_file_for_append()`
 * to lock `path`. `err` should be the `errno` set by the failing
 * call.
 */
extern void unable_to_lock_message(const char *path, int err,
				   struct strbuf *buf);

/*
 * Emit an appropriate error message and `die()` following the failure
 * of `hold_lock_file_for_update()` or `hold_lock_file_for_append()`
 * to lock `path`. `err` should be the `errno` set by the failing
 * call.
 */
extern NORETURN void unable_to_lock_die(const char *path, int err);

/*
 * Return the path of the file that is locked by the specified
 * lock_file object. The caller must free the memory.
 */
extern char *get_locked_file_path(struct lock_file *lk);

/*
 * Commit the change represented by `lk`: close the file descriptor
 * and/or file pointer if they are still open and rename the lockfile
 * to its final destination. Return 0 upon success. On failure, roll
 * back the lock file and return -1, with `errno` set to the value
 * from the failing call to `close(2)` or `rename(2)`. It is a bug to
 * call `commit_lock_file()` for a `lock_file` object that is not
 * currently locked.
 */
extern int commit_lock_file(struct lock_file *lk);

/*
 * Like `commit_lock_file()`, but rename the lockfile to the provided
 * `path`. `path` must be on the same filesystem as the lock file.
 */
extern int commit_lock_file_to(struct lock_file *lk, const char *path);

/*
 * Roll back `lk`: close the file descriptor and/or file pointer and
 * remove the lockfile. It is a NOOP to call `rollback_lock_file()`
 * for a `lock_file` object that has already been committed or rolled
 * back.
 */
extern void rollback_lock_file(struct lock_file *lk);

#endif /* LOCKFILE_H */

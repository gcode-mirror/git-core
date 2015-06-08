#include "cache.h"
#include "tempfile.h"
#include "sigchain.h"

static struct tempfile *volatile tempfile_list;

static void remove_tempfiles(int skip_fclose)
{
	pid_t me = getpid();

	while (tempfile_list) {
		if (tempfile_list->owner == me) {
			/* fclose() is not safe to call in a signal handler */
			if (skip_fclose)
				tempfile_list->fp = NULL;
			delete_tempfile(tempfile_list);
		}
		tempfile_list = tempfile_list->next;
	}
}

static void remove_tempfiles_on_exit(void)
{
	remove_tempfiles(0);
}

static void remove_tempfiles_on_signal(int signo)
{
	remove_tempfiles(1);
	sigchain_pop(signo);
	raise(signo);
}

static void register_tempfile_object(struct tempfile *tempfile, const char *path)
{
	if (!tempfile_list) {
		/* One-time initialization */
		sigchain_push_common(remove_tempfiles_on_signal);
		atexit(remove_tempfiles_on_exit);
	}

	if (tempfile->active)
		die("BUG: cannot tempfile(\"%s\") using active struct tempfile",
		    path);
	if (!tempfile->on_list) {
		/* Initialize *tempfile and add it to tempfile_list: */
		tempfile->fd = -1;
		tempfile->fp = NULL;
		tempfile->active = 0;
		tempfile->owner = 0;
		strbuf_init(&tempfile->filename, 0);
		tempfile->next = tempfile_list;
		tempfile_list = tempfile;
		tempfile->on_list = 1;
	} else if (tempfile->filename.len) {
		/* This shouldn't happen, but better safe than sorry. */
		die("BUG: tempfile(\"%s\") called with improperly-reset tempfile object",
		    path);
	}
}

/* Make sure errno contains a meaningful value on error */
int create_tempfile(struct tempfile *tempfile, const char *path)
{
	register_tempfile_object(tempfile, path);

	strbuf_add_absolute_path(&tempfile->filename, path);
	tempfile->fd = open(tempfile->filename.buf, O_RDWR | O_CREAT | O_EXCL, 0666);
	if (tempfile->fd < 0) {
		strbuf_reset(&tempfile->filename);
		return -1;
	}
	tempfile->owner = getpid();
	tempfile->active = 1;
	if (adjust_shared_perm(tempfile->filename.buf)) {
		int save_errno = errno;
		error("cannot fix permission bits on %s", tempfile->filename.buf);
		delete_tempfile(tempfile);
		errno = save_errno;
		return -1;
	}
	return tempfile->fd;
}

void register_tempfile(struct tempfile *tempfile, const char *path)
{
	register_tempfile_object(tempfile, path);

	strbuf_add_absolute_path(&tempfile->filename, path);
	tempfile->owner = getpid();
	tempfile->active = 1;
}

int mks_tempfile_sm(struct tempfile *tempfile,
		    const char *template, int suffixlen, int mode)
{
	register_tempfile_object(tempfile, template);

	strbuf_add_absolute_path(&tempfile->filename, template);
	tempfile->fd = git_mkstemps_mode(tempfile->filename.buf, suffixlen, mode);
	if (tempfile->fd < 0) {
		strbuf_reset(&tempfile->filename);
		return -1;
	}
	tempfile->owner = getpid();
	tempfile->active = 1;
	return tempfile->fd;
}

int mks_tempfile_tsm(struct tempfile *tempfile,
		     const char *template, int suffixlen, int mode)
{
	const char *tmpdir;

	register_tempfile_object(tempfile, template);

	tmpdir = getenv("TMPDIR");
	if (!tmpdir)
		tmpdir = "/tmp";

	strbuf_addf(&tempfile->filename, "%s/%s", tmpdir, template);
	tempfile->fd = git_mkstemps_mode(tempfile->filename.buf, suffixlen, mode);
	if (tempfile->fd < 0) {
		strbuf_reset(&tempfile->filename);
		return -1;
	}
	tempfile->owner = getpid();
	tempfile->active = 1;
	return tempfile->fd;
}

int xmks_tempfile_m(struct tempfile *tempfile, const char *template, int mode)
{
	int fd;
	struct strbuf full_template = STRBUF_INIT;

	strbuf_add_absolute_path(&full_template, template);
	fd = mks_tempfile_m(tempfile, full_template.buf, mode);
	if (fd < 0)
		die_errno("Unable to create temporary file '%s'",
			  full_template.buf);

	strbuf_release(&full_template);
	return fd;
}

FILE *fdopen_tempfile(struct tempfile *tempfile, const char *mode)
{
	if (!tempfile->active)
		die("BUG: fdopen_tempfile() called for unlocked object");
	if (tempfile->fp)
		die("BUG: fdopen_tempfile() called twice for file '%s'",
		    tempfile->filename.buf);

	tempfile->fp = fdopen(tempfile->fd, mode);
	return tempfile->fp;
}

int close_tempfile(struct tempfile *tempfile)
{
	int fd = tempfile->fd;
	FILE *fp = tempfile->fp;
	int err;

	if (fd < 0)
		return 0;

	tempfile->fd = -1;
	if (fp) {
		tempfile->fp = NULL;

		/*
		 * Note: no short-circuiting here; we want to fclose()
		 * in any case!
		 */
		err = ferror(fp) | fclose(fp);
	} else {
		err = close(fd);
	}

	if (err) {
		int save_errno = errno;
		delete_tempfile(tempfile);
		errno = save_errno;
		return -1;
	}

	return 0;
}

int reopen_tempfile(struct tempfile *tempfile)
{
	if (0 <= tempfile->fd)
		die(_("BUG: reopen a temporary file that is still open"));
	if (!tempfile->active)
		die(_("BUG: reopen a temporary file that has been removed"));
	tempfile->fd = open(tempfile->filename.buf, O_WRONLY);
	return tempfile->fd;
}

int rename_tempfile(struct tempfile *tempfile, const char *path)
{
	if (!tempfile->active)
		die("BUG: attempt to rename inactive temporary file to \"%s\"", path);

	if (close_tempfile(tempfile))
		return -1;

	if (rename(tempfile->filename.buf, path)) {
		int save_errno = errno;
		delete_tempfile(tempfile);
		errno = save_errno;
		return -1;
	}

	tempfile->active = 0;
	strbuf_reset(&tempfile->filename);
	return 0;
}

void delete_tempfile(struct tempfile *tempfile)
{
	if (!tempfile->active)
		return;

	if (!close_tempfile(tempfile)) {
		unlink_or_warn(tempfile->filename.buf);
		tempfile->active = 0;
		strbuf_reset(&tempfile->filename);
	}
}

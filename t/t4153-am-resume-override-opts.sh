#!/bin/sh

test_description='git-am command-line options override saved options'

. ./test-lib.sh

test_expect_success 'setup' '
	test_commit initial file &&
	test_commit first file &&

	git checkout -b side initial &&
	test_commit side-first file &&
	test_commit side-second file &&

	{
		echo "Message-Id: <side-first@example.com>" &&
		git format-patch --stdout -1 side-first | sed -e "1d"
	} >side-first.patch &&
	{
		sed -ne "1,/^\$/p" side-first.patch &&
		echo "-- >8 --" &&
		sed -e "1,/^\$/d" side-first.patch
	} >side-first.scissors &&

	{
		echo "Message-Id: <side-second@example.com>" &&
		git format-patch --stdout -1 side-second | sed -e "1d"
	} >side-second.patch &&
	{
		sed -ne "1,/^\$/p" side-second.patch &&
		echo "-- >8 --" &&
		sed -e "1,/^\$/d" side-second.patch
	} >side-second.scissors
'

test_expect_success '--3way, --no-3way' '
	rm -fr .git/rebase-apply &&
	git reset --hard &&
	git checkout first &&
	test_must_fail git am --3way side-first.patch side-second.patch &&
	test -n "$(git ls-files -u)" &&
	echo will-conflict >file &&
	git add file &&
	test_must_fail git am --no-3way --continue &&
	test -z "$(git ls-files -u)"
'

test_expect_success '--no-quiet, --quiet' '
	rm -fr .git/rebase-apply &&
	git reset --hard &&
	git checkout first &&
	test_must_fail git am --no-quiet side-first.patch side-second.patch &&
	test_must_be_empty out &&
	echo side-first >file &&
	git add file &&
	git am --quiet --continue >out &&
	test_must_be_empty out
'

test_expect_success '--signoff, --no-signoff' '
	rm -fr .git/rebase-apply &&
	git reset --hard &&
	git checkout first &&
	test_must_fail git am --signoff side-first.patch side-second.patch &&
	echo side-first >file &&
	git add file &&
	git am --no-signoff --continue &&

	# applied side-first will be signed off
	echo "Signed-off-by: $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL>" >expected &&
	git cat-file commit HEAD^ | grep "Signed-off-by:" >actual &&
	test_cmp expected actual &&

	# applied side-second will not be signed off
	test $(git cat-file commit HEAD | grep -c "Signed-off-by:") -eq 0
'

test_expect_success '--keep, --no-keep' '
	rm -fr .git/rebase-apply &&
	git reset --hard &&
	git checkout first &&
	test_must_fail git am --keep side-first.patch side-second.patch &&
	echo side-first >file &&
	git add file &&
	git am --no-keep --continue &&

	# applied side-first will keep the subject
	git cat-file commit HEAD^ >actual &&
	grep "^\[PATCH\] side-first" actual &&

	# applied side-second will not have [PATCH]
	git cat-file commit HEAD >actual &&
	! grep "^\[PATCH\] side-second" actual
'

test_expect_success '--message-id, --no-message-id' '
	rm -fr .git/rebase-apply &&
	git reset --hard &&
	git checkout first &&
	test_must_fail git am --message-id side-first.patch side-second.patch &&
	echo side-first >file &&
	git add file &&
	git am --no-message-id --continue &&

	# applied side-first will have Message-Id
	test -n "$(git cat-file commit HEAD^ | grep Message-Id)" &&

	# applied side-second will not have Message-Id
	test -z "$(git cat-file commit HEAD | grep Message-Id)"
'

test_expect_success '--scissors, --no-scissors' '
	rm -fr .git/rebase-apply &&
	git reset --hard &&
	git checkout first &&
	test_must_fail git am --scissors side-first.scissors side-second.scissors &&
	echo side-first >file &&
	git add file &&
	git am --no-scissors --continue &&

	# applied side-first will not have scissors line
	git cat-file commit HEAD^ >actual &&
	! grep "^-- >8 --" actual &&

	# applied side-second will have scissors line
	git cat-file commit HEAD >actual &&
	grep "^-- >8 --" actual
'

test_expect_success '--reject, --no-reject' '
	rm -fr .git/rebase-apply &&
	git reset --hard &&
	git checkout first &&
	rm -f file.rej &&
	test_must_fail git am --reject side-first.patch side-second.patch &&
	test_path_is_file file.rej &&
	rm -f file.rej &&
	echo will-conflict >file &&
	git add file &&
	test_must_fail git am --no-reject --continue &&
	test_path_is_missing file.rej
'

test_done

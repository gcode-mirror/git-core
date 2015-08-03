# Welcome to git development community. #

This message is written by the maintainer and talks about how Git
project is managed, and how you can work with it.

**Mailing list and the community**

The development is primarily done on the Git mailing list. Help
requests, feature proposals, bug reports and patches should be sent to
the list address <git@vger.kernel.org>.  You don't have to be
subscribed to send messages.  The convention on the list is to keep
everybody involved on Cc:, so it is unnecessary to ask "Please Cc: me,
I am not subscribed".

Before sending patches, please read Documentation/SubmittingPatches
and Documentation/CodingGuidelines to familiarize yourself with the
project convention.

If you sent a patch and you did not hear any response from anybody for
several days, it could be that your patch was totally uninteresting,
but it also is possible that it was simply lost in the noise.  Please
do not hesitate to send a reminder message in such a case.  Messages
getting lost in the noise is a sign that people involved don't have
enough mental/time bandwidth to process them right at the moment, and
it often helps to wait until the list traffic becomes calmer before
sending such a reminder.

The list archive is available at a few public sites as well:

> http://news.gmane.org/gmane.comp.version-control.git/
> http://marc.theaimsgroup.com/?l=git
> http://www.spinics.net/lists/git/

and some people seem to prefer to read it over NNTP:

> [nntp://news.gmane.org/gmane.comp.version-control.git](nntp://news.gmane.org/gmane.comp.version-control.git)

When you point at a message in a mailing list archive, using
gmane is often the easiest to follow by readers, like this:

> http://thread.gmane.org/gmane.comp.version-control.git/27/focus=217

as it also allows people who subscribe to the mailing list as gmane
newsgroup to "jump to" the article.

Some members of the development community can sometimes also be found
on the #git IRC channel on Freenode.  Its log is available at:

> http://colabti.org/irclogger/irclogger_log/git

**Reporting bugs**

When you think git does not behave as you expect, please do not stop your
bug report with just "git does not work".  "I tried to do X but it did not
work" is not much better, neither is "I tried to do X and git did Y, which
is broken".  It often is that what you expect is _not_ what other people
expect, and chances are that what you expect is very different from what
people who have worked on git have expected (otherwise, the behavior
would have been changed to match that expectation long time ago).

Please remember to always state

> - what you wanted to do;

> - what you did (the version of git and the command sequence to reproduce
> > the behavior);


> - what you saw happen;

> - what you expected to see; and

> - how the last two are different.

See http://www.chiark.greenend.org.uk/~sgtatham/bugs.html for further
hints.
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#define PROGRAM_NAME "checkpasswd"

#define BUFSIZE 4092

void Err(char *fmt, ...)
{
	char buf[BUFSIZE] = { 0 };
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), "%s", ap);
	va_end(ap);

	fprintf(stderr, "Err: %s\n", buf);

	exit(EXIT_FAILURE);
}

void trim(char *string)
{
	char *s = NULL;
	for (s = string; *s; ++s) {
		if (*s == '\n' || *s == '\r') {
			*s = '\0';
			return;
		}
	}
}

#define OS_IS_UNKNOWN 128
#define OS_IS_OPENBSD 1
#define OS_IS_LINUX 2
#define OS_IS_FREEBSD 3

static unsigned int get_os(void)
{
	char buf[BUFSIZE] = { 0 };
	FILE *p = popen("uname -s", "r");
	if (!p)
		Err("popen(): %s\n", strerror(errno));
	char *s = fgets(buf, sizeof(buf), p);
	if (!s)
		Err("fgets(): %s\n", strerror(errno));
	trim(s);

	pclose(p);

	if (!strcmp(s, "OpenBSD"))
		return OS_IS_OPENBSD;
	else if (!strcmp(s, "Linux"))
		return OS_IS_LINUX;
	else if (!strcmp(s, "FreeBSD"))
		return OS_IS_FREEBSD;

	return OS_IS_UNKNOWN;
}

void Usage(void)
{
	printf("%s <username> <guess>\n", PROGRAM_NAME);
	exit(EXIT_FAILURE);
}

#ifndef __OpenBSD__
#define _XOPEN_SOURCE
#include <unistd.h>
char *crypt(const char *key, const char *salt);

int crypt_checkpass(char *guess, char *salthash)
{
	char *result = crypt(guess, salthash);
	if (!strcmp(result, salthash))
		return 0;
	else
		return 1;
}
#endif

static unsigned int 
check_password(const char *path, char *user, char *guess)
{
	char buf[BUFSIZE] = { 0 };
	FILE *f = fopen(path, "r");
	if (!f)
		Err("fopen(): %s\n", strerror(errno));

	char *ptr = NULL;

	while ((ptr = fgets(buf, sizeof(buf), f)) != NULL) {
		trim(buf);
		char DELIM = ':';
		char *end_of_user = strchr(buf, DELIM);
		if (!end_of_user)
			Err("wtf!");

		size_t ulen = end_of_user - ptr;
		if (strncmp(user, ptr, ulen)) {		
				goto out;		
		} else {
			char *salthash = ptr + ulen + 1;
			char *end = strchr(salthash, DELIM);
			if (!end)
				Err("wtfish!");
			*end = '\0';
			
		//	printf("user %s and salthash: %s\n", user, salthash);
			int result = crypt_checkpass(guess, salthash);
			return result;
		}
	out:
		memset(buf, 0, sizeof(buf));
	}

	return 1 << 7;
}

#include <pwd.h>

int main(int argc, char **argv)
{
	char *passwd_file = NULL;
	char *username = NULL;
	char *guess = NULL;

	if (argc != 3)
		return 1 << 3;

	username = argv[1];
	guess = argv[2];

	struct passwd *pwd = getpwnam(username);
	if (pwd == NULL)
		return 1 << 4;

	/* Only allow check of user's own password */
	uid_t uid = getuid();
	if (pwd->pw_uid != uid)
		return 1 << 3;

	unsigned int os = get_os();
	switch (os) {
	case OS_IS_OPENBSD:
		passwd_file = "/etc/master.passwd";
	break;

	case OS_IS_LINUX:
		passwd_file = "/etc/shadow";
	break;

	case OS_IS_FREEBSD:
		passwd_file = "/etc/master.passwd";
	break;

	default:
		Err("Unknown OS");	
	break;
	}

	unsigned int result = check_password(passwd_file, username, guess);
	return result;
}

#include "config.h"
#include <assert.h>
#include <bitcoin/privkey.c>
#include <ccan/err/err.h>
#include <ccan/time/time.h>
#include <common/setup.h>
#include <stdio.h>
#include <unistd.h>

/* AUTOGENERATED MOCKS START */
/* Generated stub for fromwire */
const u8 *fromwire(const u8 **cursor UNNEEDED, size_t *max UNNEEDED, void *copy UNNEEDED, size_t n UNNEEDED)
{ fprintf(stderr, "fromwire called!\n"); abort(); }
/* Generated stub for towire */
void towire(u8 **pptr UNNEEDED, const void *data UNNEEDED, size_t len UNNEEDED)
{ fprintf(stderr, "towire called!\n"); abort(); }
/* AUTOGENERATED MOCKS END */
static bool verbose = false;

#define RUNS (16 * 10000)
static struct timerel const_time_test(struct secret *s1,
				      struct secret *s2,
				      size_t off)
{
	struct timeabs start, end;
	int result = 0;

	memset(s1, 0, RUNS * sizeof(*s1));
	memset(s2, 0, RUNS * sizeof(*s2));

	for (size_t i = 0; i < RUNS; i++)
		s2[i].data[off] = i;

	start = time_now();
	for (size_t i = 0; i < RUNS; i++)
		result += secret_eq_consttime(&s1[i], &s2[i]);
	end = time_now();

	if (result != RUNS / 256)
		errx(1, "Expected %u successes at offset %zu, not %u!",
		     RUNS / 256, off, result);

	return time_between(end, start);
}

static inline bool secret_eq_nonconst(const struct secret *a,
				      const struct secret *b)
{
	return memcmp(a, b, sizeof(*a)) == 0;
}

static struct timerel nonconst_time_test(struct secret *s1,
					 struct secret *s2,
					 size_t off)
{
	struct timeabs start, end;
	int result = 0;

	memset(s1, 0, RUNS * sizeof(*s1));
	memset(s2, 0, RUNS * sizeof(*s2));

	for (size_t i = 0; i < RUNS; i++)
		s2[i].data[off] = i;

	start = time_now();
	for (size_t i = 0; i < RUNS; i++)
		result += secret_eq_nonconst(&s1[i], &s2[i]);
	end = time_now();

	if (result != RUNS / 256)
		errx(1, "Expected %u successes at offset %zu, not %u!",
		     RUNS / 256, off, result);

	return time_between(end, start);
}

static struct secret *s1, *s2;

/* Returns true if test result is expected: we consider 5% "same". */
static bool secret_time_test(struct timerel (*test)(struct secret *s1,
						    struct secret *s2,
						    size_t off),
			     bool should_be_const)
{
	struct timerel firstbyte_time, lastbyte_time, diff;

	firstbyte_time = test(s1, s2, 0);
	lastbyte_time = test(s1, s2, sizeof(s1->data)-1);

	if (verbose)
		printf("First byte %u psec vs last byte %u psec\n",
		       (int)time_to_nsec(time_divide(firstbyte_time, RUNS/1000)),
		       (int)time_to_nsec(time_divide(lastbyte_time, RUNS/1000)));

	/* If they differ by more than 5%, get upset. */
	if (time_less(firstbyte_time, lastbyte_time))
		diff = time_sub(lastbyte_time, firstbyte_time);
	else {
		/* If the lastbyte test was faster, that's a fail it we expected
		 * it to be slower... */
		if (!should_be_const)
			return false;
		diff = time_sub(firstbyte_time, lastbyte_time);
	}

	return time_less(time_multiply(diff, 20), firstbyte_time) == should_be_const;
}

#define ITERATIONS 1000

int main(int argc, char *argv[])
{
	const char *v;
	int const_success, nonconst_success = ITERATIONS, i;

	common_setup(argv[0]);

	/* no point running this under valgrind. */
	v = getenv("VALGRIND");
	if (v && atoi(v) == 1)
		goto exit;

	/* this sometimes trips under CI */
	v = getenv("SLOW_MACHINE");
	if (v && atoi(v) == 1)
		goto exit;

	s1 = calloc(RUNS, sizeof(*s1));
	s2 = calloc(RUNS, sizeof(*s2));

	/* When not loaded, this should pass over 50% of the time. */
	const_success = 0;
	for (i = 0; i < ITERATIONS; i++)
		const_success += secret_time_test(const_time_test, true);

	printf("=> Within 5%% %u/%u times\n", const_success, i);

	/* This fails without -O2 or above, at least here (x86 Ubuntu gcc 7.3) */
	if (strstr(COPTFLAGS, "-O2") || strstr(COPTFLAGS, "-O3")) {
		/* Should show measurable differences at least 1/2 the time. */
		nonconst_success = 0;
		for (i = 0; i < 1000; i++)
			nonconst_success
				+= secret_time_test(nonconst_time_test, false);

		printf("=> More than 5%% slower %u/%u times\n",
		       nonconst_success, i);
	}

	/* Now, check loadavg: if we weren't alone, that could explain results */
	if (const_success < ITERATIONS / 2)
		errx(1, "Only const time %u/%u?", const_success, i);

	if (nonconst_success < ITERATIONS / 2)
		errx(1, "memcmp seemed const time %u/%u?",
		     nonconst_success, i);
	free(s1);
	free(s2);

exit:
	common_shutdown();
	return 0;
}

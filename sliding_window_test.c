#include <unit_test.h>
#include <linux/sliding_window.h>

#define WIDTH 20

#define PRINT_RESULT(slw_val) print_result(#slw_val, \
	slw_val_get(&slw, slw_val), expected[slw_val])

#define TEST_SLW_SEQUENCE(sequence, expected) ({ \
	int ret; \
	do { \
		ret = test_slw_sequence(sequence, ARRAY_SZ(sequence), \
			expected); \
	} while (0); \
	ret; \
})

static struct sliding_window slw;

enum slw_op {
	SLW_OP_IO,
	SLW_OP_RESIZE,
	SLW_OP_RESET,
	SLW_OP_NONE,
};

struct slw_io {
	enum slw_val dir;
	u32 count;
};

struct slw_size {
	u32 sz;
};

struct slw_sequence {
	enum slw_op op;
	union {
		struct slw_io io;
		struct slw_size resize;
	} action;
};

static int print_result(char *str, u32 result, u32 expected)
{
	int ok = result == expected;

	printf("total %s = %u (%s)\n", str, result, ok ? "good" : "bad");
	return !ok;
}

static int test_slw_sequence(struct slw_sequence *seq, int len, u32 expected[3])
{
	int i, ret = 0;

	for (i = 0; i < len; i++) {
		int j;

		switch (seq[i].op) {
		case SLW_OP_IO:
			for (j = 0; j < seq[i].action.io.count; j++)
				slw_advance(&slw, seq[i].action.io.dir);
			break;
		case SLW_OP_RESIZE:
			slw_resize(&slw, seq[i].action.resize.sz);
			break;
		case SLW_OP_RESET:
			slw_reset(&slw);
			break;
		case SLW_OP_NONE:
			/* fall through */
		default:
			break;
		}
	}

	if (PRINT_RESULT(SLW_NONE))
		ret = -1;
	if (PRINT_RESULT(SLW_READ))
		ret = -1;
	if (PRINT_RESULT(SLW_WRITE))
		ret = -1;

	return ret;
}

static int test_01(void)
{
	int i, ret = 0;
	u32 slw_val;
	char *result;

	/* advance slw with some read/write operations */
	for (i = 0; i < 15; i++)
		slw_advance(&slw, SLW_WRITE);

	for (i = 0; i < 1; i++)
		slw_advance(&slw, SLW_READ);

	for (i = 0; i < 5; i++)
		slw_advance(&slw, SLW_WRITE);

	for (i = 0; i < 3; i++)
		slw_advance(&slw, SLW_READ);

	/*
	                    <== sliding window <==
	  __________________________________________________________
         /                                                          |
	 |                                                          |
	 | 3 reads   5 writes   1 read                 15 writes    |
	 | _____    ___________   _   ______________________________|__________
	 |/     \  /           \ / \ /                              |          \
	 |R  R  R  W  W  W  W  W  R  W  W  W  W  W  W  W  W  W  W  W| W  W  W  W
	 |                                                          |
	 |                                                          |
	 | (newest)                                        (oldest) |
	 \__________________________________________________________| \________/

	  |                          |                             |      out
	  |                          |                             |      the
          1  ...                    10 ...                        20     window

	  current state:
	  - writes: 16
	  - reads:   4

	 */

	/* verify correct number of SLW_READ entries */
	slw_val = slw_val_get(&slw, SLW_READ);
	if (slw_val == 4) {
		result = "good";
	} else {
		result = "bad";
		ret = -1;
	}
	printf("total SLW_READ = %u (%s)\n", slw_val, result);

	/* verify correct number of SLW_WRITE entries */
	slw_val = slw_val_get(&slw, SLW_WRITE);
	if (slw_val == 16) {
		result = "good";
	} else {
		result = "bad";
		ret = -1;
	}
	printf("total SLW_WRITE = %u (%s)\n", slw_val, result);

	return ret;
}

static int test_02(void)
{
	struct slw_sequence sequence[] = {
		{ .op = SLW_OP_IO, .action.io = { SLW_WRITE, 15 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_READ, 1 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_WRITE, 5 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_READ, 3 } },
	};
	u32 expected[3] = {
		[ SLW_NONE ] = 0,
		[ SLW_READ ] = 4,
		[ SLW_WRITE ] = 16
	};

	return TEST_SLW_SEQUENCE(sequence, expected);
}

static int test_03(void)
{
	struct slw_sequence sequence[] = {
		{ .op = SLW_OP_NONE },
	};
	u32 expected[3] = {
		[ SLW_NONE ] = 20,
		[ SLW_READ ] = 0,
		[ SLW_WRITE ] = 0
	};

	return TEST_SLW_SEQUENCE(sequence, expected);
}

static int test_04(void)
{
	struct slw_sequence sequence[] = {
		{ .op = SLW_OP_IO, .action.io = { SLW_WRITE, 5 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_WRITE, 34 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_READ, 3 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_WRITE, 7 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_READ, 4 } },
	};
	u32 expected[3] = {
		[ SLW_NONE ] = 0,
		[ SLW_READ ] = 7,
		[ SLW_WRITE ] = 13
	};

	return TEST_SLW_SEQUENCE(sequence, expected);
}

static int test_05(void)
{
	struct slw_sequence sequence[] = {
		{ .op = SLW_OP_IO, .action.io = { SLW_WRITE, 5 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_WRITE, 17 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_NONE, 3 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_WRITE, 2 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_READ, 4 } },
	};
	u32 expected[3] = {
		[ SLW_NONE ] = 3,
		[ SLW_READ ] = 4,
		[ SLW_WRITE ] = 13
	};

	return TEST_SLW_SEQUENCE(sequence, expected);
}

static int test_06(void)
{
	struct slw_sequence sequence[] = {
		{ .op = SLW_OP_IO, .action.io = { SLW_WRITE, 3 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_READ, 4 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_WRITE, 5 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_READ, 2 } },
	};
	u32 expected[3] = {
		[ SLW_NONE ] = 6,
		[ SLW_READ ] = 6,
		[ SLW_WRITE ] = 8
	};

	return TEST_SLW_SEQUENCE(sequence, expected);
}

static int test_07(void)
{
	struct slw_sequence sequence[] = {
		{ .op = SLW_OP_IO, .action.io = { SLW_WRITE, 5 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_READ, 14 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_WRITE, 53 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_READ, 27 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_WRITE, 2 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_NONE, 9 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_READ, 9 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_WRITE, 17 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_READ, 4 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_NONE, 3 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_READ, 1 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_WRITE, 2 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_READ, 4 } },
	};
	u32 expected[3] = {
		[ SLW_NONE ] = 3,
		[ SLW_READ ] = 9,
		[ SLW_WRITE ] = 8
	};

	return TEST_SLW_SEQUENCE(sequence, expected);
}

static int test_08(void)
{
	struct slw_sequence sequence1[] = {
		{ .op = SLW_OP_IO, .action.io = { SLW_WRITE, 3 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_WRITE, 18 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_NONE, 6 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_WRITE, 8 } },
		{ .op = SLW_OP_IO, .action.io = { SLW_READ, 1 } },
	};
	u32 expected1[3] = {
		[ SLW_NONE ] = 6,
		[ SLW_READ ] = 1,
		[ SLW_WRITE ] = 13
	};

	struct slw_sequence sequence2[] = {
		{ .op = SLW_OP_RESET },
	};
	u32 expected2[3] = {
		[ SLW_NONE ] = 20,
		[ SLW_READ ] = 0,
		[ SLW_WRITE ] = 0,
	};

	if (TEST_SLW_SEQUENCE(sequence1, expected1))
		return -1;

	printf("\nreseting the sliding window...\n\n");

	return TEST_SLW_SEQUENCE(sequence2, expected2);

}

static int test_09(void)
{
	int widths[] = { 10, 43, 399, 401, 57, 67, 0, 3, WIDTH };
	int expected[] = { 0, 0, 0, 0, 0, 0, -EINVAL, 0, 0 };
	int ret = 0, i;
	int width = WIDTH;

	for (i = 0; i < ARRAY_SZ(widths); i++) {
		int result = slw_resize(&slw, widths[i]);

		if (result != expected[i])
			ret = -1;

		if (!result)
			width = widths[i];

		if (slw_width_get(&slw) != width)
			ret = -1;
	}

	return ret;
}

static struct single_test slw_tests[] = {
	{
		description: "Simple test (unit test demo)",
		func: test_01,
	},
	{
		description: "Basic test: 4 R/W bursts",
		func: test_02,
	},
	{
		description: "No reads and no writes",
		func: test_03,
	},
	{
		description: "Basic test: 6 R/W bursts",
		func: test_04,
	},
	{
		description: "R/W bursts combined with NONE bursts",
		func: test_05,
	},
	{
		description: "Less R/W than window width",
		func: test_06,
	},
	{
		description: "Intensive combined test",
		func: test_07,
	},
	{
		description: "Reset test",
		func: test_08,
	},
	{
		description: "Resize test",
		func: test_09,
	},
};

static int slw_tests_init(void)
{
	int ret;

	printf("slw_init(&slw, %d)\n", WIDTH);
	ret = slw_init(&slw, WIDTH);

	printf("slw_width_get(&slw): %u\n", slw_width_get(&slw));
	return ret;
}

static int slw_tests_uninit(void)
{
	printf("slw_uninit(&slw)\n");
	slw_uninit(&slw);
	return 0;
}

static int slw_pre_test(void)
{
	int ret;

	ret = slw_resize(&slw, WIDTH);
	slw_reset(&slw);

	return ret;
}

struct unit_test ut_slw = {
	.module = "slw",
	.description = "Sliding Window",
	.pre_all_tests = slw_tests_init,
	.post_all_tests = slw_tests_uninit,
	.pre_single_test = slw_pre_test,
	.tests = slw_tests,
	.count = ARRAY_SZ(slw_tests),
};


#define _XOPEN_SOURCE

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define ASSERT(x, ...) do { \
    if (!(x)) { \
        fprintf(stderr, "assertion error: "); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        __builtin_trap(); \
    } \
} while (0)

struct operand {
    enum {OP_DATE, OP_DURATION} tag;
    struct tm val;
};

enum operator {
    OP_ADD,
    OP_SUB,
};

static struct tm today = {0};

static void init_today(void)
{
    time_t t = time(0);
    ASSERT(t >= 0, "time value overflow");
    today = *gmtime(&t);
}

static int parse_date(char *src, struct operand *dst)
{
    if (strcmp(src, "TODAY") == 0) {
        dst->tag = OP_DATE;
        dst->val = today;
        return 0;
    }

    struct tm t = {0};

    char *after_date = strptime(src, "%Y-%m-%d", &t);
    if (after_date == 0) {
        t.tm_year = today.tm_year;
        t.tm_mon = today.tm_mon;
        t.tm_mday = today.tm_mday;
    }
    char *after_time = strptime(after_date ? after_date : src, "%H:%M:%S", &t);
    if (after_date == 0 && after_time == 0)
        return -1;
    strptime(after_time ? after_time : src, "%z", &t);

    dst->tag = OP_DATE;
    dst->val = t;
    return 0;
}

static int parse_duration(char *src, struct operand *dst)
{
    #define SV_EAT(lhs_ident, rhs_static) \
        (strncmp(lhs_ident, rhs_static, sizeof(rhs_static)-1) == 0 && (lhs_ident = lhs_ident + sizeof(rhs_static)-1))

    for (int val, skip;;) {
        int ret = sscanf(src, "%d%n",  &val,  &skip);
        ASSERT(!ferror(stdin), "sscanf error: %s", strerror(errno));
        if (ret < 0) break;

        src += skip;

        if /**/ (SV_EAT(src, "years"))  dst->val.tm_year += val;
        else if (SV_EAT(src, "months")) dst->val.tm_mon  += val;
        else if (SV_EAT(src, "weeks"))  dst->val.tm_mday += val * 7;
        else if (SV_EAT(src, "days"))   dst->val.tm_mday += val;
        else if (SV_EAT(src, "hours"))  dst->val.tm_hour += val;
        else if (SV_EAT(src, "mins"))   dst->val.tm_min  += val;
        else if (SV_EAT(src, "secs"))   dst->val.tm_sec  += val;
        else return -1;
    }

    dst->tag = OP_DURATION;
    return 0;
}

static int parse_operand(char *src, struct operand *dst)
{
    if (parse_date(src, dst)     == 0) return 0;
    if (parse_duration(src, dst) == 0) return 0;
    return -1;
}

static int parse_operator(char *src, enum operator *dst)
{
    if (strcmp(src, "+") == 0) { *dst = OP_ADD; return 0; }
    if (strcmp(src, "-") == 0) { *dst = OP_SUB; return 0; }
    return -1;
}

static char *shift_args(int *argc, char ***argv)
{
    if (*argc == 0)
        return 0;
    char *arg = (*argv)[0];
    *argc -= 1;
    *argv += 1;
    return arg;
}

int main(int argc, char **argv)
{
    init_today();

    char *program = shift_args(&argc, &argv);

    #define DIE_USAGE(msg_static) do { \
        fprintf(stderr, "error: " msg_static "\n"); \
        fprintf(stderr, "Try `%s` for usage information.\n", program); \
        return 1; \
    } while (0)

    if (argc == 0) {
        fprintf(stderr, "USAGE\n");
        fprintf(stderr, "   Add duration to date:\n");
        fprintf(stderr, "       %s DATE + DURATION\n", program);
        fprintf(stderr, "\n");
        fprintf(stderr, "   Substract duration from date:\n");
        fprintf(stderr, "       %s DATE - DURATION\n", program);
        fprintf(stderr, "\n");
        fprintf(stderr, "   Duration between dates:\n");
        fprintf(stderr, "       %s DATE - DATE\n", program);
        fprintf(stderr, "\n");
        fprintf(stderr, "   Add durations:\n");
        fprintf(stderr, "       %s DURATION + DURATION\n", program);
        fprintf(stderr, "\n");
        fprintf(stderr, "   Substract durations:\n");
        fprintf(stderr, "       %s DURATION - DURATION\n", program);
        fprintf(stderr, "\n");
        fprintf(stderr, "SYNTAX\n");
        fprintf(stderr, "   DATE := [YYYY-MM-DD] [[hh:mm:ss] [(+|-)hh:mm]]\n");
        fprintf(stderr, "         | TODAY\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "   DURATION := [%%years] [%%months] [%%weeks]\n");
        fprintf(stderr, "               [%%days] [%%hours] [%%mins] [%%secs]\n");
        return 0;
    }

    char *lhs_src = shift_args(&argc, &argv);
    char *op_src = shift_args(&argc, &argv);
    if (op_src == 0) DIE_USAGE("expected `+` or `-`");
    char *rhs_src = shift_args(&argc, &argv);
    if (rhs_src == 0) DIE_USAGE("expected right operand");

    struct operand lhs = {0};
    struct operand rhs = {0};
    enum operator op;

    if (parse_operand(lhs_src, &lhs) < 0) DIE_USAGE("left operand is illformed");
    if (parse_operator(op_src, &op) < 0)  DIE_USAGE("operator is illformed");
    if (parse_operand(rhs_src, &rhs) < 0) DIE_USAGE("right operand is illformed");

    switch (lhs.tag) {
    case OP_DATE:
        switch (rhs.tag) {
        case OP_DATE:
            switch (op) {
            case OP_ADD:
                DIE_USAGE("can't add dates");

            case OP_SUB:
                lhs.val.tm_year -= rhs.val.tm_year;
                lhs.val.tm_mon  -= rhs.val.tm_mon;
                lhs.val.tm_mday -= rhs.val.tm_mday;
                lhs.val.tm_hour -= rhs.val.tm_hour;
                lhs.val.tm_min  -= rhs.val.tm_min;
                lhs.val.tm_sec  -= rhs.val.tm_sec;
                lhs.tag = OP_DURATION;
                break;
            }
            break;

        case OP_DURATION:
            switch (op) {
            case OP_ADD:
                lhs.val.tm_year += rhs.val.tm_year;
                lhs.val.tm_mon  += rhs.val.tm_mon;
                lhs.val.tm_mday += rhs.val.tm_mday;
                lhs.val.tm_hour += rhs.val.tm_hour;
                lhs.val.tm_min  += rhs.val.tm_min;
                lhs.val.tm_sec  += rhs.val.tm_sec;
                break;

            case OP_SUB:
                lhs.val.tm_year -= rhs.val.tm_year;
                lhs.val.tm_mon  -= rhs.val.tm_mon;
                lhs.val.tm_mday -= rhs.val.tm_mday;
                lhs.val.tm_hour -= rhs.val.tm_hour;
                lhs.val.tm_min  -= rhs.val.tm_min;
                lhs.val.tm_sec  -= rhs.val.tm_sec;
                break;
            }
            break;
        }
        break;

    case OP_DURATION:
        switch (rhs.tag) {
        case OP_DATE:
            DIE_USAGE("can't do arithmetic between duration and date");

        case OP_DURATION:
            switch (op) {
            case OP_ADD:
                lhs.val.tm_year += rhs.val.tm_year;
                lhs.val.tm_mon  += rhs.val.tm_mon;
                lhs.val.tm_mday += rhs.val.tm_mday;
                lhs.val.tm_hour += rhs.val.tm_hour;
                lhs.val.tm_min  += rhs.val.tm_min;
                lhs.val.tm_sec  += rhs.val.tm_sec;
                break;

            case OP_SUB:
                lhs.val.tm_year -= rhs.val.tm_year;
                lhs.val.tm_mon  -= rhs.val.tm_mon;
                lhs.val.tm_mday -= rhs.val.tm_mday;
                lhs.val.tm_hour -= rhs.val.tm_hour;
                lhs.val.tm_min  -= rhs.val.tm_min;
                lhs.val.tm_sec  -= rhs.val.tm_sec;
                break;
            }
            break;
        }
        break;
    }

    switch (lhs.tag) {
    case OP_DATE:
        ASSERT(mktime(&lhs.val) >= 0, "time value overflow");
        fprintf(stderr, "date: %s\n", asctime(&lhs.val));
        break;

    case OP_DURATION:
        fprintf(stderr, "DURATION:\n");
        fprintf(stderr, "year: %d\n", lhs.val.tm_year);
        fprintf(stderr, "mont: %d\n", lhs.val.tm_mon);
        fprintf(stderr, "days: %d\n", lhs.val.tm_mday);
        fprintf(stderr, "hour: %d\n", lhs.val.tm_hour);
        fprintf(stderr, "mins: %d\n", lhs.val.tm_min);
        fprintf(stderr, "secs: %d\n", lhs.val.tm_sec);
        break;
    }

    return 0;
}

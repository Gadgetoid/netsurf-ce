#include <getopt.h>
#include <string.h>

char *optarg;
int optind = 1;
int opterr = 1;
int optopt;

static int next_character;

static int short_option(int argc, char *const argv[], const char *options) {
    if (!next_character) {
        next_character = 1;
    }
    const char *argument = argv[optind];
    int option = argument[next_character++];
    const char *found = strchr(options, option);
    optarg = NULL;
    if (!found || option == ':') {
        optopt = option;
        if (!argument[next_character]) {
            optind++;
            next_character = 0;
        }
        return '?';
    }
    if (found[1] == ':') {
        if (argument[next_character]) {
            optarg = (char *)&argument[next_character];
        } else if (found[2] != ':' && optind + 1 < argc) {
            optarg = argv[++optind];
        } else if (found[2] != ':') {
            optopt = option;
            optind++;
            next_character = 0;
            return options[0] == ':' ? ':' : '?';
        }
        optind++;
        next_character = 0;
        return option;
    }
    if (!argument[next_character]) {
        optind++;
        next_character = 0;
    }
    return option;
}

static int long_option(int argc, char *const argv[], const struct option *long_options, int *index) {
    const char *name = argv[optind] + 2;
    const char *value = strchr(name, '=');
    size_t length = value ? (size_t)(value - name) : strlen(name);
    optind++;
    optarg = NULL;
    for (int candidate = 0; long_options[candidate].name; candidate++) {
        const struct option *option = &long_options[candidate];
        if (strncmp(option->name, name, length) != 0 || option->name[length]) {
            continue;
        }
        if (index) {
            *index = candidate;
        }
        if (option->has_arg != no_argument) {
            if (value) {
                optarg = (char *)value + 1;
            } else if (option->has_arg == required_argument) {
                if (optind >= argc) {
                    return '?';
                }
                optarg = argv[optind++];
            }
        }
        if (option->flag) {
            *option->flag = option->val;
            return 0;
        }
        return option->val;
    }
    return '?';
}

int getopt_long(int argc, char *const argv[], const char *options, const struct option *long_options, int *index) {
    if (!next_character) {
        if (optind >= argc || argv[optind][0] != '-' || !argv[optind][1]) {
            return -1;
        }
        if (strcmp(argv[optind], "--") == 0) {
            optind++;
            return -1;
        }
        if (argv[optind][1] == '-' && long_options) {
            return long_option(argc, argv, long_options, index);
        }
    }
    return short_option(argc, argv, options);
}

int getopt(int argc, char *const argv[], const char *options) {
    return getopt_long(argc, argv, options, NULL, NULL);
}

#ifndef CE_POSIX_ASSERT_H
#define CE_POSIX_ASSERT_H

void ce_assert_failed(const char *expression, const char *file, int line);

#endif

#undef assert
#ifdef NDEBUG
#define assert(expression) ((void)0)
#else
#define assert(expression) ((expression) ? (void)0 : ce_assert_failed(#expression, __FILE__, __LINE__))
#endif

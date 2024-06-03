#ifndef ASSERTION_UTIL_H
#define ASSERTION_UTIL_H

#ifndef NDEBUG
#define ASSERT_EX(condition, statement) \
  do {                                  \
    if (!(condition)) {                 \
      statement;                        \
      assert(condition);                \
    }                                   \
  } while (false)
#else
#define ASSERT_EX(condition, statement) ((void)0)
#endif

#endif /* ASSERTION_UTIL_H */

#ifndef SAFE_MEMORY_H
#define SAFE_MEMORY_H

#include <stddef.h>

#include <string.h>
#include <zephyr/sys/__assert.h>

static inline void safe_memcpy(void *dst, size_t dst_len,
			       const void *src, size_t copy_len)
{
	__ASSERT_NO_MSG(dst != NULL);
	__ASSERT_NO_MSG(src != NULL);
	__ASSERT_NO_MSG(copy_len <= dst_len);
	(void)memcpy(dst, src, copy_len);
}

static inline void safe_memset(void *dst, size_t dst_len, int value, size_t set_len)
{
	__ASSERT_NO_MSG(dst != NULL);
	__ASSERT_NO_MSG(set_len <= dst_len);
	(void)memset(dst, value, set_len);
}

static inline size_t safe_strlen(const char *str, size_t max_len)
{
	size_t len = 0U;

	__ASSERT_NO_MSG(str != NULL);
	while ((len < max_len) && (str[len] != '\0')) {
		len++;
	}

	__ASSERT_NO_MSG(len < max_len);
	return len;
}

#endif /* SAFE_MEMORY_H */

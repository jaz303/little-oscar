#ifndef OSC_H
#define OSC_H

/*
 * OSC pattern-matching routines based on 'OSC Message Dispatching and Pattern Matching',
 * (http://opensoundcontrol.org/spec-1_0)
 *
 * Character ranges (e.g. [abc], [!abc], [a-z], [!a-z]) are currently unsupported.
 */

typedef struct osc_pattern {
    int         is_static;
    const char  *pattern;
} osc_pattern_t;

enum {
    OSC_PATTERN_INVALID         = 0,
    OSC_PATTERN_STATIC          = 1,
    OSC_PATTERN_DYNAMIC         = 2
};

/*
 * Verify that a pattern string is valid. The following rules are enforced:
 *
 * - leading slash
 * - no trailing slash 
 * - no empty parts
 * - no illegal characters (#)
 * - no unmatched brackets and braces
 * - no nested braces
 *
 * @param pattern - pattern string to verify
 * @return 1 if pattern is valid, 0 otherwise
 */
int osc_pattern_verify(const char *pattern);

/*
 * Compile a pattern
 *
 * For now this function simply scans the pattern for metacharacters, allowing
 * `osc_pattern_match()` to use a simple `strcmp()` in static cases. In the future
 * compilation maybe be
 *
 * @param pattern_out - an `osc_pattern_t` in which to store compiled pattern
 * @param pattern_in - pattern string. this is *not* copied so you *must* ensure this pointer
 *        remains valid for the lifetime of the compiled pattern.
 * @return 1 if pattern was compiled successfully, 0 otherwise
 */
int osc_pattern_compile(osc_pattern_t *pattern_out, const char *pattern_in);

/*
 * Match a string against a pattern
 *
 * @param pattern - an `osc_pattern_t` to match against
 * @param input - string to match 
 * @return 1 if string matches pattern, 0 otherwise
 */
int osc_pattern_match(osc_pattern_t *pattern, const char *input);

#endif
#include <stdio.h>
#include <mach/mach_time.h>

#include "pattern.h"

osc_pattern_t compiled;

void should_not_match(const char *pattern, const char *input) {
    osc_pattern_compile(&compiled, pattern);
    if (!osc_pattern_match(&compiled, input)) {
        printf("[ OK ] `%s` does not match pattern `%s`\n", input, pattern);
    } else {
        printf("[FAIL] `%s` matches pattern `%s`\n", input, pattern);
    }
}

void should_match(const char *pattern, const char *input) {
    osc_pattern_compile(&compiled, pattern);
    if (osc_pattern_match(&compiled, input)) {
        printf("[ OK ] `%s` matches pattern `%s`\n", input, pattern);
    } else {
        printf("[FAIL] `%s` does not match pattern `%s`\n", input, pattern);
    }
}

int main(int argc, char *argv) {
    
    should_match    ("/foo",                    "/foo");
    should_match    ("/foo/bar",                "/foo/bar");
    should_match    ("/foo/bar/baz",            "/foo/bar/baz");
    
    should_match    ("/?",                      "/a");
    should_match    ("/?",                      "/b");
    should_not_match("/?",                      "/ab");
    should_not_match("/?",                      "/abc");
    
    should_match    ("/a?c",                    "/abc");
    should_match    ("/a?c",                    "/acc");
    should_match    ("/a?c",                    "/a1c");
    should_match    ("/?a?",                    "/dad");
    should_match    ("/?a?",                    "/bad");
    
    should_match    ("/*",                      "/foo");
    should_match    ("/*",                      "/bar");
    should_match    ("/*/*",                    "/bar/bleem");
    should_match    ("/*/*",                    "/bar/boof");
    should_match    ("/foo/*",                  "/foo/abc");
    should_match    ("/foo/*",                  "/foo/magic");
    should_match    ("/foo/*/bar",              "/foo/abc/bar");
    should_match    ("/foo/*/bar",              "/foo/defghi/bar");
    should_match    ("/foo/*/*/bar",            "/foo/abc/def/bar");
    
    should_match    ("/foo*",                   "/foo");
    should_match    ("/foo*",                   "/foobar");
    should_match    ("/foo*",                   "/foof");
    
    should_match    ("/foo*/bar",               "/foo/bar");
    should_match    ("/foo*/bar",               "/foobar/bar");
    should_match    ("/foo*/bar",               "/foof/bar");
    
    should_match    ("/???/foo/*",              "/abc/foo/a");
    should_match    ("/???/foo/*",              "/abc/foo/ab");
    should_match    ("/???/foo/*",              "/abc/foo/abc");
    
    //
    // Choice
    
    should_match    ("/{a,b,c}",                 "/a");
    should_match    ("/{a,b,c}",                 "/b");
    should_match    ("/{a,b,c}",                 "/c");
    should_not_match("/{a,b,c}",                 "/d");
    
    should_match    ("/{a,b,c}/boof",            "/a/boof");
    should_match    ("/{a,b,c}/barf",            "/b/barf");
    should_match    ("/{a,b,c}/snarf",           "/c/snarf");
    should_not_match("/{a,b,c}/darth",           "/d/darth");
    
    should_match    ("/{foo,bar,baz}",          "/foo");
    should_match    ("/{foo,bar,baz}",          "/bar");
    should_match    ("/{foo,bar,baz}",          "/baz");
    should_not_match("/{foo,bar,baz}",          "/zip");
    should_not_match("/{foo,bar,baz}",          "/fof");
    
    should_match    ("/{foo,bar,baz}zoom",      "/foozoom");
    should_match    ("/{foo,bar,baz}zoom",      "/barzoom");
    should_match    ("/{foo,bar,baz}zoom",      "/bazzoom");
    should_not_match("/{foo,bar,baz}zoom",      "/zimzoom");
    
    should_match    ("/a/{b,c,d,e}/f",          "/a/b/f");
    should_match    ("/a/{b,c,d,e}/f",          "/a/c/f");
    should_match    ("/a/{b,c,d,e}/f",          "/a/d/f");
    should_match    ("/a/{b,c,d,e}/f",          "/a/e/f");
    
    should_match    ("/a/{b1,c23,d456,e789}/f", "/a/b1/f");
    should_match    ("/a/{b1,c23,d456,e789}/f", "/a/c23/f");
    should_match    ("/a/{b1,c23,d456,e789}/f", "/a/d456/f");
    should_match    ("/a/{b1,c23,d456,e789}/f", "/a/e789/f");
    should_not_match("/a/{b1,c23,d456,e789}/f", "/a/e78/f");
    
    should_match    ("/a{abc,def,ghi}b",        "/aabcb");
    should_match    ("/a{abc,def,ghi}b",        "/adefb");
    should_match    ("/a{abc,def,ghi}b",        "/aghib");
    should_not_match("/a{abc,def,ghi}b",        "/aabb");
    should_not_match("/a{abc,def,ghi}b",        "/adeb");
    should_not_match("/a{abc,def,ghi}b",        "/aghb");
    
    should_match    ("/a/{a12,b23,c34}/{d56,e67,f89}",  "/a/a12/d56");
    should_match    ("/a/{a12,b23,c34}/{d56,e67,f89}",  "/a/b23/d56");
    should_match    ("/a/{a12,b23,c34}/{d56,e67,f89}",  "/a/c34/d56");
    should_match    ("/a/{a12,b23,c34}/{d56,e67,f89}",  "/a/a12/e67");
    should_match    ("/a/{a12,b23,c34}/{d56,e67,f89}",  "/a/b23/e67");
    should_match    ("/a/{a12,b23,c34}/{d56,e67,f89}",  "/a/c34/e67");
    should_match    ("/a/{a12,b23,c34}/{d56,e67,f89}",  "/a/a12/f89");
    should_match    ("/a/{a12,b23,c34}/{d56,e67,f89}",  "/a/b23/f89");
    should_match    ("/a/{a12,b23,c34}/{d56,e67,f89}",  "/a/c34/f89");
    
    should_match    ("/a{alpha,bravo,delta}*",          "/aalpha");
    should_match    ("/a{alpha,bravo,delta}*",          "/aalpha1");
    should_match    ("/a{alpha,bravo,delta}*",          "/abravo");
    should_match    ("/a{alpha,bravo,delta}*",          "/abravo1");
    
    
    //
    // Invalid patterns
    
    should_not_match("/*/foo",                  "//foo");
    
    
    // Quick speed test
    
    
    
    
    return 0;
}
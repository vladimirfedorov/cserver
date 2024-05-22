#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../cserver.h" 

// Test definitions
int test_string_init() {
    printf("- test_string_init ");
    string str = string_init();
    if (str.value != NULL || str.length != 0) {
        printf("failed.\n");
        return 1;
    }
    printf("OK\n");
    return 0;
}

int test_string_make() {
    printf("- test_string_make ");
    const char *test_str = "Hello, World!";
    string str = string_make(test_str);
    if (str.value == NULL || strcmp(str.value, test_str) != 0 || str.length != strlen(test_str)) {
        printf("failed.\n");
        return 1;
    }
    string_free(str);
    printf("OK\n");
    return 0;
}

int test_read_file() {
    printf("- test_read_file ");
    // Test with an existing file
    string file_content = read_file("testfile.txt");
    if (file_content.value == NULL) {
        printf("failed.\n");
        return 1;
    }
    string_free(file_content);

    // Test with a non-existing file
    file_content = read_file("nonexistentfile.txt");
    if (file_content.value != NULL || file_content.length != 0) {
        printf("failed.\n");
        return 1;
    }
    printf("OK\n");
    return 0;
}

int test_make_response() {
    printf("- test_make_response ");
    string content = string_make("Hello, World!");
    string response = make_response("200 OK", "text/plain", content);
    if (response.value == NULL) {
        string_free(response);
        string_free(content);
        printf("failed.\n");
        return 1;
    }
    if (strstr(response.value, "HTTP/1.1 200 OK") == NULL ||
        strstr(response.value, "Content-Type: text/plain") == NULL ||
        strstr(response.value, "Content-Length: 13") == NULL ||
        strstr(response.value, "\r\nHello, World!") == NULL) {
        string_free(response);
        string_free(content);
        printf("failed.\n");
        return 1;
    }
    string_free(response);
    string_free(content);
    printf("OK\n");
    return 0;
}

// Example tests for the added functions
int test_get_content_type() {
    printf("- test_get_content_type ");
    if (strcmp(get_content_type("/", "index.html"), content_type_html) != 0 ||
        strcmp(get_content_type("/", "index.md"), content_type_html) != 0 ||
        strcmp(get_content_type("/", "index.json"), content_type_json) != 0 ||
        strcmp(get_content_type("/", "index.txt"), content_type_text) != 0) {
        printf("failed.\n");
        return 1;
    }
    printf("OK\n");
    return 0;
}

int test_rendering(char *name) {  

  printf("- %s ", name);

  // Markdown test name
  char md_filename[256];
  snprintf(md_filename, sizeof(md_filename), "%s.md", name);
  string md_content = read_file(md_filename);
  if (md_content.value == NULL) {
    printf("failed: %s not found.\n", md_filename);
    return 1;
  }

  // Result test name
  char html_filename[256];
  snprintf(html_filename, sizeof(html_filename), "%s.html", name);
  string html_content = read_file(html_filename);
  if (html_content.value == NULL) {
    printf("failed: %s not found.\n", html_filename);
    return 1;
  }

  cJSON *context = cJSON_CreateObject();
  string rendered_content = render_page(context, md_filename);
  if (rendered_content.value == NULL) {
    printf("failed: no result.\n");
    return 1;
  }

  if (strcmp(html_content.value, rendered_content.value) != 0) {
    printf("failed: values don't match:\nExpected:\n%s\nRendered:\n%s\n", html_content.value, rendered_content.value);
    string_free(rendered_content);
    string_free(md_content);
    string_free(html_content);
    return 1;
  }

  string_free(rendered_content);
  string_free(md_content);
  string_free(html_content);

  printf("OK\n");
  return 0;
}

int main() {

  printf("Running cserver tests...\n");
  int total = 8;
  int failed = 0;

  failed += test_string_init();
  failed += test_string_make();
  failed += test_read_file();
  failed += test_make_response();
  failed += test_get_content_type();
  failed += test_rendering("test-md-metadata");
  failed += test_rendering("test-md-only");
  failed += test_rendering("test-metadata-only");

  printf("%i tests run, %i failed.\n", total, failed);
  return failed;
}
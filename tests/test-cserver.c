#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../cserver.h" 


int test_rendering(char *name) {  

  printf("- %s ", name);

  // Markdown test name
  char md_filename[256];
  snprintf(md_filename, sizeof(md_filename), "%s.md", name);
  string md_content = read_file(md_filename);
  if (md_content.value == NULL) {
    printf("failed: %s not found.\n", md_filename);
    return -1;
  }

  // Result test name
  char html_filename[256];
  snprintf(html_filename, sizeof(html_filename), "%s.html", name);
  string html_content = read_file(html_filename);
  if (html_content.value == NULL) {
    printf("failed: %s not found.\n", html_filename);
    return -1;
  }

  cJSON *context = cJSON_CreateObject();
  string rendered_content = render_page(context, md_filename);
  if (rendered_content.value == NULL) {
    printf("failed: no result.\n");
    return -1;
  }

  if (strcmp(html_content.value, rendered_content.value) != 0) {
    printf("failed: values don't match:\nExpected:\n%s\nRendered:\n%s\n", html_content.value, rendered_content.value);
    string_free(rendered_content);
    string_free(md_content);
    string_free(html_content);
    return -1;
  }

  string_free(rendered_content);
  string_free(md_content);
  string_free(html_content);

  printf("OK\n");
  return 0;
}

int main() {

  printf("cserver tests...\n");

  test_rendering("test-md-metadata");
  test_rendering("test-md-only");
  test_rendering("test-metadata-only");

  printf("Done.\n");
  return 0;
}
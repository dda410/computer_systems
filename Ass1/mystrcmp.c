#include <stdio.h> 

int mystrcmp(char *first_input_string, char *second_input_string) {
  int index = 0;
  while (1) {
    /*The first if statement checks whether the two strings have the same character at the same position*/
    if (first_input_string[index] != second_input_string[index]) {
      return 0;
    }
    /*If the null character is met then the two strings are equal since all the other characters are the same*/
    if (first_input_string[index] == '\0') {
      return 1;
    }
      index++;
  }
}

int main(int argc, char **argv) {
    int comparison_result;
    if (argc != 3) {
        printf("Usage: mystrcmp <input_first_string_with_no_space_inside_it> <input_second_string_with_no_space_inside_it>\n\n");
        return 1;
    }
    comparison_result = mystrcmp(argv[1], argv[2]);
    if (comparison_result == 0) {
      printf("The two strings are different.\n");
    } else {
      printf("The two strings are identical.\n");
    }
    return 0;
}

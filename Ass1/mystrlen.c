#include <stdio.h> 

int mystrlen(char *input_string) {
  int index = 0;
  while (1) {
    /*In c a string terminates with the '\0' character.
      The index of this character will tell us also the length of the string.*/
    if (input_string[index] == '\0') {
      break;
    }
    index++;
  }
  return index;
}

int main(int argc, char **argv) {
  int length;
  if (argc != 2) {
    printf("Usage: strlen <input_string_with_no_space_inside_it>\n\n");
    return 1;
  }
  length = mystrlen(argv[1]);
  printf("The length is: %d characters.\n", length);
  return 0;
}

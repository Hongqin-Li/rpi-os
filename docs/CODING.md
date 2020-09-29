# Kernel Coding Style

This document presents the preferred coding style for our kernel.

## Indentation

- Indentation via four spaces, not tabs.

## Braces and Spaces

- The general style is similar to the K&R style such as:
  ```
  int
  main()
  {
      struct a {
          ...
      };
      
      for (int i = 0; i < n; i ++) {
          ...
      }

      while (...) {
          ...
      }

      do {
          ...
      } while (...);

      if (...) {
          ...
      } else if (...) {
          ...
      } else
          ...
  }
  ```

- Align `switch` and its `case` labels within the same column such as:
  ```
  switch (a) {
  case 1:
  case 2:
  default:
      break;
  }
  ```

- Use one space around binary and ternary operators.
  For example, `a ? b : c` not `a ? b: c`

- No space after unary operators such as `& * ~ ! __attribute__`.
  For example, `~p` not `~ p`.

- No space before the postfix and prefix increment/decrement unary operators.
  For example, `++a` not `++ a`.

- No space after the name of a function in a call.
  For example, `printf("hello")` not `printf ("hello")`.

- Pointer types have spaces.
  For example, `(uint16_t *)` not `(uint16_t*)`.

## Comments

- Comments are usually C `/* ... */` comments.

- The preferred style for Multi-line comments is:
  ```
  /*
   * A column of asterisks on the left side,
   * with beginning and ending almost-blank lines.
   */
  ```
## Naming

- Preprocessor macros are always UPPERCASE except:
  `assert`, `panic`, `static_assert`, `offsetof`, `container_of`

- Function and variable names are all lower-case separated by underscores.

- In a function definition, the function name starts a new line.
  Then you can `grep -n '^foo' */*.c` to find the definition of `foo`.

- Functions that take no arguments are declared `f()` not `f(void)`.

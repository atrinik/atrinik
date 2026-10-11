int old_copy(void) { return 1; }
int new_copy(void) { return 2; }
__asm__(".symver old_copy,copy@ABI_1");
__asm__(".symver new_copy,copy@@ABI_2");
